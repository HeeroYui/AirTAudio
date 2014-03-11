/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */


#if defined(__WINDOWS_ASIO__) // ASIO API on Windows

#include <airtaudio/Interface.h>

airtaudio::Api* airtaudio::api::Asio::Create(void) {
	return new airtaudio::api::Asio();
}


// The ASIO API is designed around a callback scheme, so this
// implementation is similar to that used for OS-X CoreAudio and Linux
// Jack.	The primary constraint with ASIO is that it only allows
// access to a single driver at a time.	Thus, it is not possible to
// have more than one simultaneous RtAudio stream.
//
// This implementation also requires a number of external ASIO files
// and a few global variables.	The ASIO callback scheme does not
// allow for the passing of user data, so we must create a global
// pointer to our callbackInfo structure.
//
// On unix systems, we make use of a pthread condition variable.
// Since there is no equivalent in Windows, I hacked something based
// on information found in
// http://www.cs.wustl.edu/~schmidt/win32-cv-1.html.

#include "asiosys.h"
#include "asio.h"
#include "iasiothiscallresolver.h"
#include "asiodrivers.h"
#include <cmath>

static AsioDrivers drivers;
static ASIOCallbacks asioCallbacks;
static ASIODriverInfo driverInfo;
static CallbackInfo *asioCallbackInfo;
static bool asioXRun;

struct AsioHandle {
	int32_t drainCounter;			 // Tracks callback counts when draining
	bool internalDrain;		 // Indicates if stop is initiated from callback or not.
	ASIOBufferInfo *bufferInfos;
	HANDLE condition;

	AsioHandle()
		:drainCounter(0), internalDrain(false), bufferInfos(0) {}
};

// Function declarations (definitions at end of section)
static const char* getAsioErrorString(ASIOError result);
static void sampleRateChanged(ASIOSampleRate sRate);
static long asioMessages(long selector, long value, void* message, double* opt);

airtaudio::api::Asio::Asio(void) {
	// ASIO cannot run on a multi-threaded appartment. You can call
	// CoInitialize beforehand, but it must be for appartment threading
	// (in which case, CoInitilialize will return S_FALSE here).
	m_coInitialized = false;
	HRESULT hr = CoInitialize(NULL); 
	if (FAILED(hr)) {
		m_errorText = "airtaudio::api::Asio::ASIO requires a single-threaded appartment. Call CoInitializeEx(0,COINIT_APARTMENTTHREADED)";
		error(airtaudio::errorWarning);
	}
	m_coInitialized = true;

	drivers.removeCurrentDriver();
	driverInfo.asioVersion = 2;

	// See note in DirectSound implementation about GetDesktopWindow().
	driverInfo.sysRef = GetForegroundWindow();
}

airtaudio::api::Asio::~Asio(void) {
	if (m_stream.state != STREAM_CLOSED) closeStream();
	if (m_coInitialized) CoUninitialize();
}

uint32_t airtaudio::api::Asio::getDeviceCount(void)
{
	return (uint32_t) drivers.asioGetNumDev();
}

rtaudio::DeviceInfo airtaudio::api::Asio::getDeviceInfo(uint32_t device)
{
	rtaudio::DeviceInfo info;
	info.probed = false;

	// Get device ID
	uint32_t nDevices = getDeviceCount();
	if (nDevices == 0) {
		m_errorText = "airtaudio::api::Asio::getDeviceInfo: no devices found!";
		error(airtaudio::errorInvalidUse);
		return info;
	}

	if (device >= nDevices) {
		m_errorText = "airtaudio::api::Asio::getDeviceInfo: device ID is invalid!";
		error(airtaudio::errorInvalidUse);
		return info;
	}

	// If a stream is already open, we cannot probe other devices.	Thus, use the saved results.
	if (m_stream.state != STREAM_CLOSED) {
		if (device >= m_devices.size()) {
			m_errorText = "airtaudio::api::Asio::getDeviceInfo: device ID was not present before stream was opened.";
			error(airtaudio::errorWarning);
			return info;
		}
		return m_devices[ device ];
	}

	char driverName[32];
	ASIOError result = drivers.asioGetDriverName((int) device, driverName, 32);
	if (result != ASE_OK) {
		m_errorStream << "airtaudio::api::Asio::getDeviceInfo: unable to get driver name (" << getAsioErrorString(result) << ").";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}

	info.name = driverName;

	if (!drivers.loadDriver(driverName)) {
		m_errorStream << "airtaudio::api::Asio::getDeviceInfo: unable to load driver (" << driverName << ").";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}

	result = ASIOInit(&driverInfo);
	if (result != ASE_OK) {
		m_errorStream << "airtaudio::api::Asio::getDeviceInfo: error (" << getAsioErrorString(result) << ") initializing driver (" << driverName << ").";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}

	// Determine the device channel information.
	long inputChannels, outputChannels;
	result = ASIOGetChannels(&inputChannels, &outputChannels);
	if (result != ASE_OK) {
		drivers.removeCurrentDriver();
		m_errorStream << "airtaudio::api::Asio::getDeviceInfo: error (" << getAsioErrorString(result) << ") getting channel count (" << driverName << ").";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}

	info.outputChannels = outputChannels;
	info.inputChannels = inputChannels;
	if (info.outputChannels > 0 && info.inputChannels > 0)
		info.duplexChannels = (info.outputChannels > info.inputChannels) ? info.inputChannels : info.outputChannels;

	// Determine the supported sample rates.
	info.sampleRates.clear();
	for (uint32_t i=0; i<MAX_SAMPLE_RATES; i++) {
		result = ASIOCanSampleRate((ASIOSampleRate) SAMPLE_RATES[i]);
		if (result == ASE_OK)
			info.sampleRates.push_back(SAMPLE_RATES[i]);
	}

	// Determine supported data types ... just check first channel and assume rest are the same.
	ASIOChannelInfo channelInfo;
	channelInfo.channel = 0;
	channelInfo.isInput = true;
	if (info.inputChannels <= 0) channelInfo.isInput = false;
	result = ASIOGetChannelInfo(&channelInfo);
	if (result != ASE_OK) {
		drivers.removeCurrentDriver();
		m_errorStream << "airtaudio::api::Asio::getDeviceInfo: error (" << getAsioErrorString(result) << ") getting driver channel info (" << driverName << ").";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}

	info.nativeFormats = 0;
	if (channelInfo.type == ASIOSTInt16MSB || channelInfo.type == ASIOSTInt16LSB)
		info.nativeFormats |= RTAUDIO_SINT16;
	else if (channelInfo.type == ASIOSTInt32MSB || channelInfo.type == ASIOSTInt32LSB)
		info.nativeFormats |= RTAUDIO_SINT32;
	else if (channelInfo.type == ASIOSTFloat32MSB || channelInfo.type == ASIOSTFloat32LSB)
		info.nativeFormats |= RTAUDIO_FLOAT32;
	else if (channelInfo.type == ASIOSTFloat64MSB || channelInfo.type == ASIOSTFloat64LSB)
		info.nativeFormats |= RTAUDIO_FLOAT64;
	else if (channelInfo.type == ASIOSTInt24MSB || channelInfo.type == ASIOSTInt24LSB)
		info.nativeFormats |= RTAUDIO_SINT24;

	if (info.outputChannels > 0)
		if (getDefaultOutputDevice() == device) info.isDefaultOutput = true;
	if (info.inputChannels > 0)
		if (getDefaultInputDevice() == device) info.isDefaultInput = true;

	info.probed = true;
	drivers.removeCurrentDriver();
	return info;
}

static void bufferSwitch(long index, ASIOBool processNow)
{
	RtApiAsio *object = (RtApiAsio *) asioCallbackInfo->object;
	object->callbackEvent(index);
}

void airtaudio::api::Asio::saveDeviceInfo(void)
{
	m_devices.clear();

	uint32_t nDevices = getDeviceCount();
	m_devices.resize(nDevices);
	for (uint32_t i=0; i<nDevices; i++)
		m_devices[i] = getDeviceInfo(i);
}

bool airtaudio::api::Asio::probeDeviceOpen(uint32_t device, StreamMode mode, uint32_t channels,
																	 uint32_t firstChannel, uint32_t sampleRate,
																	 rtaudio::format format, uint32_t *bufferSize,
																	 rtaudio::StreamOptions *options)
{
	// For ASIO, a duplex stream MUST use the same driver.
	if (mode == INPUT && m_stream.mode == OUTPUT && m_stream.device[0] != device) {
		m_errorText = "airtaudio::api::Asio::probeDeviceOpen: an ASIO duplex stream must use the same device for input and output!";
		return FAILURE;
	}

	char driverName[32];
	ASIOError result = drivers.asioGetDriverName((int) device, driverName, 32);
	if (result != ASE_OK) {
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: unable to get driver name (" << getAsioErrorString(result) << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Only load the driver once for duplex stream.
	if (mode != INPUT || m_stream.mode != OUTPUT) {
		// The getDeviceInfo() function will not work when a stream is open
		// because ASIO does not allow multiple devices to run at the same
		// time.	Thus, we'll probe the system before opening a stream and
		// save the results for use by getDeviceInfo().
		this->saveDeviceInfo();

		if (!drivers.loadDriver(driverName)) {
			m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: unable to load driver (" << driverName << ").";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		result = ASIOInit(&driverInfo);
		if (result != ASE_OK) {
			m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: error (" << getAsioErrorString(result) << ") initializing driver (" << driverName << ").";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}
	}

	// Check the device channel count.
	long inputChannels, outputChannels;
	result = ASIOGetChannels(&inputChannels, &outputChannels);
	if (result != ASE_OK) {
		drivers.removeCurrentDriver();
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: error (" << getAsioErrorString(result) << ") getting channel count (" << driverName << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	if ((mode == OUTPUT && (channels+firstChannel) > (uint32_t) outputChannels) ||
			 (mode == INPUT && (channels+firstChannel) > (uint32_t) inputChannels)) {
		drivers.removeCurrentDriver();
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: driver (" << driverName << ") does not support requested channel count (" << channels << ") + offset (" << firstChannel << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	m_stream.nDeviceChannels[mode] = channels;
	m_stream.nUserChannels[mode] = channels;
	m_stream.channelOffset[mode] = firstChannel;

	// Verify the sample rate is supported.
	result = ASIOCanSampleRate((ASIOSampleRate) sampleRate);
	if (result != ASE_OK) {
		drivers.removeCurrentDriver();
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: driver (" << driverName << ") does not support requested sample rate (" << sampleRate << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Get the current sample rate
	ASIOSampleRate currentRate;
	result = ASIOGetSampleRate(&currentRate);
	if (result != ASE_OK) {
		drivers.removeCurrentDriver();
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: driver (" << driverName << ") error getting sample rate.";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Set the sample rate only if necessary
	if (currentRate != sampleRate) {
		result = ASIOSetSampleRate((ASIOSampleRate) sampleRate);
		if (result != ASE_OK) {
			drivers.removeCurrentDriver();
			m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: driver (" << driverName << ") error setting sample rate (" << sampleRate << ").";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}
	}

	// Determine the driver data type.
	ASIOChannelInfo channelInfo;
	channelInfo.channel = 0;
	if (mode == OUTPUT) channelInfo.isInput = false;
	else channelInfo.isInput = true;
	result = ASIOGetChannelInfo(&channelInfo);
	if (result != ASE_OK) {
		drivers.removeCurrentDriver();
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: driver (" << driverName << ") error (" << getAsioErrorString(result) << ") getting data format.";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Assuming WINDOWS host is always little-endian.
	m_stream.doByteSwap[mode] = false;
	m_stream.userFormat = format;
	m_stream.deviceFormat[mode] = 0;
	if (channelInfo.type == ASIOSTInt16MSB || channelInfo.type == ASIOSTInt16LSB) {
		m_stream.deviceFormat[mode] = RTAUDIO_SINT16;
		if (channelInfo.type == ASIOSTInt16MSB) m_stream.doByteSwap[mode] = true;
	}
	else if (channelInfo.type == ASIOSTInt32MSB || channelInfo.type == ASIOSTInt32LSB) {
		m_stream.deviceFormat[mode] = RTAUDIO_SINT32;
		if (channelInfo.type == ASIOSTInt32MSB) m_stream.doByteSwap[mode] = true;
	}
	else if (channelInfo.type == ASIOSTFloat32MSB || channelInfo.type == ASIOSTFloat32LSB) {
		m_stream.deviceFormat[mode] = RTAUDIO_FLOAT32;
		if (channelInfo.type == ASIOSTFloat32MSB) m_stream.doByteSwap[mode] = true;
	}
	else if (channelInfo.type == ASIOSTFloat64MSB || channelInfo.type == ASIOSTFloat64LSB) {
		m_stream.deviceFormat[mode] = RTAUDIO_FLOAT64;
		if (channelInfo.type == ASIOSTFloat64MSB) m_stream.doByteSwap[mode] = true;
	}
	else if (channelInfo.type == ASIOSTInt24MSB || channelInfo.type == ASIOSTInt24LSB) {
		m_stream.deviceFormat[mode] = RTAUDIO_SINT24;
		if (channelInfo.type == ASIOSTInt24MSB) m_stream.doByteSwap[mode] = true;
	}

	if (m_stream.deviceFormat[mode] == 0) {
		drivers.removeCurrentDriver();
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: driver (" << driverName << ") data format not supported by RtAudio.";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Set the buffer size.	For a duplex stream, this will end up
	// setting the buffer size based on the input constraints, which
	// should be ok.
	long minSize, maxSize, preferSize, granularity;
	result = ASIOGetBufferSize(&minSize, &maxSize, &preferSize, &granularity);
	if (result != ASE_OK) {
		drivers.removeCurrentDriver();
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: driver (" << driverName << ") error (" << getAsioErrorString(result) << ") getting buffer size.";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	if (*bufferSize < (uint32_t) minSize) *bufferSize = (uint32_t) minSize;
	else if (*bufferSize > (uint32_t) maxSize) *bufferSize = (uint32_t) maxSize;
	else if (granularity == -1) {
		// Make sure bufferSize is a power of two.
		int32_t log2_of_min_size = 0;
		int32_t log2_of_max_size = 0;

		for (uint32_t i = 0; i < sizeof(long) * 8; i++) {
			if (minSize & ((long)1 << i)) log2_of_min_size = i;
			if (maxSize & ((long)1 << i)) log2_of_max_size = i;
		}

		long min_delta = std::abs((long)*bufferSize - ((long)1 << log2_of_min_size));
		int32_t min_delta_num = log2_of_min_size;

		for (int32_t i = log2_of_min_size + 1; i <= log2_of_max_size; i++) {
			long current_delta = std::abs((long)*bufferSize - ((long)1 << i));
			if (current_delta < min_delta) {
				min_delta = current_delta;
				min_delta_num = i;
			}
		}

		*bufferSize = ((uint32_t)1 << min_delta_num);
		if (*bufferSize < (uint32_t) minSize) *bufferSize = (uint32_t) minSize;
		else if (*bufferSize > (uint32_t) maxSize) *bufferSize = (uint32_t) maxSize;
	}
	else if (granularity != 0) {
		// Set to an even multiple of granularity, rounding up.
		*bufferSize = (*bufferSize + granularity-1) / granularity * granularity;
	}

	if (mode == INPUT && m_stream.mode == OUTPUT && m_stream.bufferSize != *bufferSize) {
		drivers.removeCurrentDriver();
		m_errorText = "airtaudio::api::Asio::probeDeviceOpen: input/output buffersize discrepancy!";
		return FAILURE;
	}

	m_stream.bufferSize = *bufferSize;
	m_stream.nBuffers = 2;

	if (options && options->flags & RTAUDIO_NONINTERLEAVED) m_stream.userInterleaved = false;
	else m_stream.userInterleaved = true;

	// ASIO always uses non-interleaved buffers.
	m_stream.deviceInterleaved[mode] = false;

	// Allocate, if necessary, our AsioHandle structure for the stream.
	AsioHandle *handle = (AsioHandle *) m_stream.apiHandle;
	if (handle == 0) {
		try {
			handle = new AsioHandle;
		}
		catch (std::bad_alloc&) {
			//if (handle == NULL) {		
			drivers.removeCurrentDriver();
			m_errorText = "airtaudio::api::Asio::probeDeviceOpen: error allocating AsioHandle memory.";
			return FAILURE;
		}
		handle->bufferInfos = 0;

		// Create a manual-reset event.
		handle->condition = CreateEvent(NULL,	 // no security
																		 TRUE,	 // manual-reset
																		 FALSE,	// non-signaled initially
																		 NULL); // unnamed
		m_stream.apiHandle = (void *) handle;
	}

	// Create the ASIO internal buffers.	Since RtAudio sets up input
	// and output separately, we'll have to dispose of previously
	// created output buffers for a duplex stream.
	long inputLatency, outputLatency;
	if (mode == INPUT && m_stream.mode == OUTPUT) {
		ASIODisposeBuffers();
		if (handle->bufferInfos) free(handle->bufferInfos);
	}

	// Allocate, initialize, and save the bufferInfos in our stream callbackInfo structure.
	bool buffersAllocated = false;
	uint32_t i, nChannels = m_stream.nDeviceChannels[0] + m_stream.nDeviceChannels[1];
	handle->bufferInfos = (ASIOBufferInfo *) malloc(nChannels * sizeof(ASIOBufferInfo));
	if (handle->bufferInfos == NULL) {
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: error allocating bufferInfo memory for driver (" << driverName << ").";
		m_errorText = m_errorStream.str();
		goto error;
	}

	ASIOBufferInfo *infos;
	infos = handle->bufferInfos;
	for (i=0; i<m_stream.nDeviceChannels[0]; i++, infos++) {
		infos->isInput = ASIOFalse;
		infos->channelNum = i + m_stream.channelOffset[0];
		infos->buffers[0] = infos->buffers[1] = 0;
	}
	for (i=0; i<m_stream.nDeviceChannels[1]; i++, infos++) {
		infos->isInput = ASIOTrue;
		infos->channelNum = i + m_stream.channelOffset[1];
		infos->buffers[0] = infos->buffers[1] = 0;
	}

	// Set up the ASIO callback structure and create the ASIO data buffers.
	asioCallbacks.bufferSwitch = &bufferSwitch;
	asioCallbacks.sampleRateDidChange = &sampleRateChanged;
	asioCallbacks.asioMessage = &asioMessages;
	asioCallbacks.bufferSwitchTimeInfo = NULL;
	result = ASIOCreateBuffers(handle->bufferInfos, nChannels, m_stream.bufferSize, &asioCallbacks);
	if (result != ASE_OK) {
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: driver (" << driverName << ") error (" << getAsioErrorString(result) << ") creating buffers.";
		m_errorText = m_errorStream.str();
		goto error;
	}
	buffersAllocated = true;

	// Set flags for buffer conversion.
	m_stream.doConvertBuffer[mode] = false;
	if (m_stream.userFormat != m_stream.deviceFormat[mode])
		m_stream.doConvertBuffer[mode] = true;
	if (m_stream.userInterleaved != m_stream.deviceInterleaved[mode] &&
			 m_stream.nUserChannels[mode] > 1)
		m_stream.doConvertBuffer[mode] = true;

	// Allocate necessary internal buffers
	uint64_t bufferBytes;
	bufferBytes = m_stream.nUserChannels[mode] * *bufferSize * formatBytes(m_stream.userFormat);
	m_stream.userBuffer[mode] = (char *) calloc(bufferBytes, 1);
	if (m_stream.userBuffer[mode] == NULL) {
		m_errorText = "airtaudio::api::Asio::probeDeviceOpen: error allocating user buffer memory.";
		goto error;
	}

	if (m_stream.doConvertBuffer[mode]) {

		bool makeBuffer = true;
		bufferBytes = m_stream.nDeviceChannels[mode] * formatBytes(m_stream.deviceFormat[mode]);
		if (mode == INPUT) {
			if (m_stream.mode == OUTPUT && m_stream.deviceBuffer) {
				uint64_t bytesOut = m_stream.nDeviceChannels[0] * formatBytes(m_stream.deviceFormat[0]);
				if (bufferBytes <= bytesOut) makeBuffer = false;
			}
		}

		if (makeBuffer) {
			bufferBytes *= *bufferSize;
			if (m_stream.deviceBuffer) free(m_stream.deviceBuffer);
			m_stream.deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_stream.deviceBuffer == NULL) {
				m_errorText = "airtaudio::api::Asio::probeDeviceOpen: error allocating device buffer memory.";
				goto error;
			}
		}
	}

	m_stream.sampleRate = sampleRate;
	m_stream.device[mode] = device;
	m_stream.state = STREAM_STOPPED;
	asioCallbackInfo = &m_stream.callbackInfo;
	m_stream.callbackInfo.object = (void *) this;
	if (m_stream.mode == OUTPUT && mode == INPUT)
		// We had already set up an output stream.
		m_stream.mode = DUPLEX;
	else
		m_stream.mode = mode;

	// Determine device latencies
	result = ASIOGetLatencies(&inputLatency, &outputLatency);
	if (result != ASE_OK) {
		m_errorStream << "airtaudio::api::Asio::probeDeviceOpen: driver (" << driverName << ") error (" << getAsioErrorString(result) << ") getting latency.";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning); // warn but don't fail
	}
	else {
		m_stream.latency[0] = outputLatency;
		m_stream.latency[1] = inputLatency;
	}

	// Setup the buffer conversion information structure.	We don't use
	// buffers to do channel offsets, so we override that parameter
	// here.
	if (m_stream.doConvertBuffer[mode]) setConvertInfo(mode, 0);

	return SUCCESS;

 error:
	if (buffersAllocated)
		ASIODisposeBuffers();
	drivers.removeCurrentDriver();

	if (handle) {
		CloseHandle(handle->condition);
		if (handle->bufferInfos)
			free(handle->bufferInfos);
		delete handle;
		m_stream.apiHandle = 0;
	}

	for (int32_t i=0; i<2; i++) {
		if (m_stream.userBuffer[i]) {
			free(m_stream.userBuffer[i]);
			m_stream.userBuffer[i] = 0;
		}
	}

	if (m_stream.deviceBuffer) {
		free(m_stream.deviceBuffer);
		m_stream.deviceBuffer = 0;
	}

	return FAILURE;
}

void airtaudio::api::Asio::closeStream()
{
	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Asio::closeStream(): no open stream to close!";
		error(airtaudio::errorWarning);
		return;
	}

	if (m_stream.state == STREAM_RUNNING) {
		m_stream.state = STREAM_STOPPED;
		ASIOStop();
	}
	ASIODisposeBuffers();
	drivers.removeCurrentDriver();

	AsioHandle *handle = (AsioHandle *) m_stream.apiHandle;
	if (handle) {
		CloseHandle(handle->condition);
		if (handle->bufferInfos)
			free(handle->bufferInfos);
		delete handle;
		m_stream.apiHandle = 0;
	}

	for (int32_t i=0; i<2; i++) {
		if (m_stream.userBuffer[i]) {
			free(m_stream.userBuffer[i]);
			m_stream.userBuffer[i] = 0;
		}
	}

	if (m_stream.deviceBuffer) {
		free(m_stream.deviceBuffer);
		m_stream.deviceBuffer = 0;
	}

	m_stream.mode = UNINITIALIZED;
	m_stream.state = STREAM_CLOSED;
}

bool stopThreadCalled = false;

void airtaudio::api::Asio::startStream()
{
	verifyStream();
	if (m_stream.state == STREAM_RUNNING) {
		m_errorText = "airtaudio::api::Asio::startStream(): the stream is already running!";
		error(airtaudio::errorWarning);
		return;
	}

	AsioHandle *handle = (AsioHandle *) m_stream.apiHandle;
	ASIOError result = ASIOStart();
	if (result != ASE_OK) {
		m_errorStream << "airtaudio::api::Asio::startStream: error (" << getAsioErrorString(result) << ") starting device.";
		m_errorText = m_errorStream.str();
		goto unlock;
	}

	handle->drainCounter = 0;
	handle->internalDrain = false;
	ResetEvent(handle->condition);
	m_stream.state = STREAM_RUNNING;
	asioXRun = false;

 unlock:
	stopThreadCalled = false;

	if (result == ASE_OK) return;
	error(airtaudio::errorSystemError);
}

void airtaudio::api::Asio::stopStream()
{
	verifyStream();
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Asio::stopStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}

	AsioHandle *handle = (AsioHandle *) m_stream.apiHandle;
	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {
		if (handle->drainCounter == 0) {
			handle->drainCounter = 2;
			WaitForSingleObject(handle->condition, INFINITE);	// block until signaled
		}
	}

	m_stream.state = STREAM_STOPPED;

	ASIOError result = ASIOStop();
	if (result != ASE_OK) {
		m_errorStream << "airtaudio::api::Asio::stopStream: error (" << getAsioErrorString(result) << ") stopping device.";
		m_errorText = m_errorStream.str();
	}

	if (result == ASE_OK) return;
	error(airtaudio::errorSystemError);
}

void airtaudio::api::Asio::abortStream()
{
	verifyStream();
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Asio::abortStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}

	// The following lines were commented-out because some behavior was
	// noted where the device buffers need to be zeroed to avoid
	// continuing sound, even when the device buffers are completely
	// disposed.	So now, calling abort is the same as calling stop.
	// AsioHandle *handle = (AsioHandle *) m_stream.apiHandle;
	// handle->drainCounter = 2;
	stopStream();
}

// This function will be called by a spawned thread when the user
// callback function signals that the stream should be stopped or
// aborted.	It is necessary to handle it this way because the
// callbackEvent() function must return before the ASIOStop()
// function will return.
static unsigned __stdcall asioStopStream(void *ptr)
{
	CallbackInfo *info = (CallbackInfo *) ptr;
	RtApiAsio *object = (RtApiAsio *) info->object;

	object->stopStream();
	_endthreadex(0);
	return 0;
}

bool airtaudio::api::Asio::callbackEvent(long bufferIndex)
{
	if (m_stream.state == STREAM_STOPPED || m_stream.state == STREAM_STOPPING) return SUCCESS;
	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Asio::callbackEvent(): the stream is closed ... this shouldn't happen!";
		error(airtaudio::errorWarning);
		return FAILURE;
	}

	CallbackInfo *info = (CallbackInfo *) &m_stream.callbackInfo;
	AsioHandle *handle = (AsioHandle *) m_stream.apiHandle;

	// Check if we were draining the stream and signal if finished.
	if (handle->drainCounter > 3) {

		m_stream.state = STREAM_STOPPING;
		if (handle->internalDrain == false)
			SetEvent(handle->condition);
		else { // spawn a thread to stop the stream
			unsigned threadId;
			m_stream.callbackInfo.thread = _beginthreadex(NULL, 0, &asioStopStream,
																										&m_stream.callbackInfo, 0, &threadId);
		}
		return SUCCESS;
	}

	// Invoke user callback to get fresh output data UNLESS we are
	// draining stream.
	if (handle->drainCounter == 0) {
		airtaudio::AirTAudioCallback callback = (airtaudio::AirTAudioCallback) info->callback;
		double streamTime = getStreamTime();
		rtaudio::streamStatus status = 0;
		if (m_stream.mode != INPUT && asioXRun == true) {
			status |= RTAUDIO_OUTPUT_UNDERFLOW;
			asioXRun = false;
		}
		if (m_stream.mode != OUTPUT && asioXRun == true) {
			status |= RTAUDIO_INPUT_OVERFLOW;
			asioXRun = false;
		}
		int32_t cbReturnValue = callback(m_stream.userBuffer[0], m_stream.userBuffer[1],
																		 m_stream.bufferSize, streamTime, status, info->userData);
		if (cbReturnValue == 2) {
			m_stream.state = STREAM_STOPPING;
			handle->drainCounter = 2;
			unsigned threadId;
			m_stream.callbackInfo.thread = _beginthreadex(NULL, 0, &asioStopStream,
																										&m_stream.callbackInfo, 0, &threadId);
			return SUCCESS;
		}
		else if (cbReturnValue == 1) {
			handle->drainCounter = 1;
			handle->internalDrain = true;
		}
	}

	uint32_t nChannels, bufferBytes, i, j;
	nChannels = m_stream.nDeviceChannels[0] + m_stream.nDeviceChannels[1];
	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {

		bufferBytes = m_stream.bufferSize * formatBytes(m_stream.deviceFormat[0]);

		if (handle->drainCounter > 1) { // write zeros to the output stream

			for (i=0, j=0; i<nChannels; i++) {
				if (handle->bufferInfos[i].isInput != ASIOTrue)
					memset(handle->bufferInfos[i].buffers[bufferIndex], 0, bufferBytes);
			}

		}
		else if (m_stream.doConvertBuffer[0]) {

			convertBuffer(m_stream.deviceBuffer, m_stream.userBuffer[0], m_stream.convertInfo[0]);
			if (m_stream.doByteSwap[0])
				byteSwapBuffer(m_stream.deviceBuffer,
												m_stream.bufferSize * m_stream.nDeviceChannels[0],
												m_stream.deviceFormat[0]);

			for (i=0, j=0; i<nChannels; i++) {
				if (handle->bufferInfos[i].isInput != ASIOTrue)
					memcpy(handle->bufferInfos[i].buffers[bufferIndex],
									&m_stream.deviceBuffer[j++*bufferBytes], bufferBytes);
			}

		}
		else {

			if (m_stream.doByteSwap[0])
				byteSwapBuffer(m_stream.userBuffer[0],
												m_stream.bufferSize * m_stream.nUserChannels[0],
												m_stream.userFormat);

			for (i=0, j=0; i<nChannels; i++) {
				if (handle->bufferInfos[i].isInput != ASIOTrue)
					memcpy(handle->bufferInfos[i].buffers[bufferIndex],
									&m_stream.userBuffer[0][bufferBytes*j++], bufferBytes);
			}

		}

		if (handle->drainCounter) {
			handle->drainCounter++;
			goto unlock;
		}
	}

	if (m_stream.mode == INPUT || m_stream.mode == DUPLEX) {

		bufferBytes = m_stream.bufferSize * formatBytes(m_stream.deviceFormat[1]);

		if (m_stream.doConvertBuffer[1]) {

			// Always interleave ASIO input data.
			for (i=0, j=0; i<nChannels; i++) {
				if (handle->bufferInfos[i].isInput == ASIOTrue)
					memcpy(&m_stream.deviceBuffer[j++*bufferBytes],
									handle->bufferInfos[i].buffers[bufferIndex],
									bufferBytes);
			}

			if (m_stream.doByteSwap[1])
				byteSwapBuffer(m_stream.deviceBuffer,
												m_stream.bufferSize * m_stream.nDeviceChannels[1],
												m_stream.deviceFormat[1]);
			convertBuffer(m_stream.userBuffer[1], m_stream.deviceBuffer, m_stream.convertInfo[1]);

		}
		else {
			for (i=0, j=0; i<nChannels; i++) {
				if (handle->bufferInfos[i].isInput == ASIOTrue) {
					memcpy(&m_stream.userBuffer[1][bufferBytes*j++],
									handle->bufferInfos[i].buffers[bufferIndex],
									bufferBytes);
				}
			}

			if (m_stream.doByteSwap[1])
				byteSwapBuffer(m_stream.userBuffer[1],
												m_stream.bufferSize * m_stream.nUserChannels[1],
												m_stream.userFormat);
		}
	}

 unlock:
	// The following call was suggested by Malte Clasen.	While the API
	// documentation indicates it should not be required, some device
	// drivers apparently do not function correctly without it.
	ASIOOutputReady();

	RtApi::tickStreamTime();
	return SUCCESS;
}

static void sampleRateChanged(ASIOSampleRate sRate)
{
	// The ASIO documentation says that this usually only happens during
	// external sync.	Audio processing is not stopped by the driver,
	// actual sample rate might not have even changed, maybe only the
	// sample rate status of an AES/EBU or S/PDIF digital input at the
	// audio device.

	RtApi *object = (RtApi *) asioCallbackInfo->object;
	try {
		object->stopStream();
	}
	catch (RtError &exception) {
		std::cerr << "\nRtApiAsio: sampleRateChanged() error (" << exception.getMessage() << ")!\n" << std::endl;
		return;
	}

	std::cerr << "\nRtApiAsio: driver reports sample rate changed to " << sRate << " ... stream stopped!!!\n" << std::endl;
}

static long asioMessages(long selector, long value, void* message, double* opt)
{
	long ret = 0;

	switch(selector) {
	case kAsioSelectorSupported:
		if (value == kAsioResetRequest
				 || value == kAsioEngineVersion
				 || value == kAsioResyncRequest
				 || value == kAsioLatenciesChanged
				 // The following three were added for ASIO 2.0, you don't
				 // necessarily have to support them.
				 || value == kAsioSupportsTimeInfo
				 || value == kAsioSupportsTimeCode
				 || value == kAsioSupportsInputMonitor)
			ret = 1L;
		break;
	case kAsioResetRequest:
		// Defer the task and perform the reset of the driver during the
		// next "safe" situation.	You cannot reset the driver right now,
		// as this code is called from the driver.	Reset the driver is
		// done by completely destruct is. I.e. ASIOStop(),
		// ASIODisposeBuffers(), Destruction Afterwards you initialize the
		// driver again.
		std::cerr << "\nRtApiAsio: driver reset requested!!!" << std::endl;
		ret = 1L;
		break;
	case kAsioResyncRequest:
		// This informs the application that the driver encountered some
		// non-fatal data loss.	It is used for synchronization purposes
		// of different media.	Added mainly to work around the Win16Mutex
		// problems in Windows 95/98 with the Windows Multimedia system,
		// which could lose data because the Mutex was held too long by
		// another thread.	However a driver can issue it in other
		// situations, too.
		// std::cerr << "\nRtApiAsio: driver resync requested!!!" << std::endl;
		asioXRun = true;
		ret = 1L;
		break;
	case kAsioLatenciesChanged:
		// This will inform the host application that the drivers were
		// latencies changed.	Beware, it this does not mean that the
		// buffer sizes have changed!	You might need to update internal
		// delay data.
		std::cerr << "\nRtApiAsio: driver latency may have changed!!!" << std::endl;
		ret = 1L;
		break;
	case kAsioEngineVersion:
		// Return the supported ASIO version of the host application.	If
		// a host application does not implement this selector, ASIO 1.0
		// is assumed by the driver.
		ret = 2L;
		break;
	case kAsioSupportsTimeInfo:
		// Informs the driver whether the
		// asioCallbacks.bufferSwitchTimeInfo() callback is supported.
		// For compatibility with ASIO 1.0 drivers the host application
		// should always support the "old" bufferSwitch method, too.
		ret = 0;
		break;
	case kAsioSupportsTimeCode:
		// Informs the driver whether application is interested in time
		// code info.	If an application does not need to know about time
		// code, the driver has less work to do.
		ret = 0;
		break;
	}
	return ret;
}

static const char* getAsioErrorString(ASIOError result)
{
	struct Messages 
	{
		ASIOError value;
		const char*message;
	};

	static const Messages m[] = 
		{
			{	 ASE_NotPresent,		"Hardware input or output is not present or available." },
			{	 ASE_HWMalfunction,	"Hardware is malfunctioning." },
			{	 ASE_InvalidParameter, "Invalid input parameter." },
			{	 ASE_InvalidMode,			"Invalid mode." },
			{	 ASE_SPNotAdvancing,		 "Sample position not advancing." },
			{	 ASE_NoClock,						"Sample clock or rate cannot be determined or is not present." },
			{	 ASE_NoMemory,					 "Not enough memory to complete the request." }
		};

	for (uint32_t i = 0; i < sizeof(m)/sizeof(m[0]); ++i)
		if (m[i].value == result) return m[i].message;

	return "Unknown error.";
}
//******************** End of __WINDOWS_ASIO__ *********************//
#endif
