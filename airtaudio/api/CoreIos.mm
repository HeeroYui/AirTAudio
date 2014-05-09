/**
 * @author Edouard DUPIN
 * 
 * @license like MIT (see license file)
 */

#ifdef __IOS_CORE__

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>

#include <unistd.h>
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>
#include <limits.h>

airtaudio::Api* airtaudio::api::CoreIos::Create(void) {
	ATA_INFO("Create CoreIos device ... ");
	return new airtaudio::api::CoreIos();
}


// ============================================================================================================

@interface IosAudioController : NSObject {
	AudioComponentInstance audioUnit;
	AudioBuffer tempBuffer; // this will hold the latest data from the microphone
}

@property (readonly) AudioComponentInstance audioUnit;
@property (readonly) AudioBuffer tempBuffer;

- (void) start;
- (void) stop;
- (void) processAudio: (AudioBufferList*) bufferList;

@end


// ============================================================================================================

#define kOutputBus 0
#define kInputBus 1

IosAudioController* iosAudio;

void checkStatus(int status){
	if (status) {
		printf("Status not 0! %d\n", status);
		//		exit(1);
	}
}

/**
 This callback is called when new audio data from the microphone is
 available.
 */
static OSStatus recordingCallback(void *inRefCon, 
                                  AudioUnitRenderActionFlags *ioActionFlags, 
                                  const AudioTimeStamp *inTimeStamp, 
                                  UInt32 inBusNumber, 
                                  UInt32 inNumberFrames, 
                                  AudioBufferList *ioData) {
	// Because of the way our audio format (setup below) is chosen:
	// we only need 1 buffer, since it is mono
	// Samples are 16 bits = 2 bytes.
	// 1 frame includes only 1 sample
	
	AudioBuffer buffer;
	
	buffer.mNumberChannels = 1;
	buffer.mDataByteSize = inNumberFrames * 2;
	buffer.mData = malloc( inNumberFrames * 2 );
	
	// Put buffer in a AudioBufferList
	AudioBufferList bufferList;
	bufferList.mNumberBuffers = 1;
	bufferList.mBuffers[0] = buffer;
	
    // Then:
    // Obtain recorded samples
	
    OSStatus status;
	
    status = AudioUnitRender([iosAudio audioUnit], 
                             ioActionFlags, 
                             inTimeStamp, 
                             inBusNumber, 
                             inNumberFrames, 
                             &bufferList);
	checkStatus(status);
	
    // Now, we have the samples we just read sitting in buffers in bufferList
	// Process the new data
	[iosAudio processAudio:&bufferList];
	
	// release the malloc'ed data in the buffer we created earlier
	free(bufferList.mBuffers[0].mData);
	
    return noErr;
}

/**
 This callback is called when the audioUnit needs new data to play through the
 speakers. If you don't have any, just don't write anything in the buffers
 */
static OSStatus playbackCallback(void *inRefCon, 
								 AudioUnitRenderActionFlags *ioActionFlags, 
								 const AudioTimeStamp *inTimeStamp, 
								 UInt32 inBusNumber, 
								 UInt32 inNumberFrames, 
								 AudioBufferList *ioData) {    
    // Notes: ioData contains buffers (may be more than one!)
    // Fill them up as much as you can. Remember to set the size value in each buffer to match how
    // much data is in the buffer.
	
	for (int i=0; i < ioData->mNumberBuffers; i++) { // in practice we will only ever have 1 buffer, since audio format is mono
		AudioBuffer buffer = ioData->mBuffers[i];
		
		//		NSLog(@"  Buffer %d has %d channels and wants %d bytes of data.", i, buffer.mNumberChannels, buffer.mDataByteSize);
		
		// copy temporary buffer data to output buffer
		UInt32 size = std::min(buffer.mDataByteSize, [iosAudio tempBuffer].mDataByteSize); // dont copy more data then we have, or then fits
		memcpy(buffer.mData, [iosAudio tempBuffer].mData, size);
		buffer.mDataByteSize = size; // indicate how much data we wrote in the buffer
		
		// uncomment to hear random noise
		/*
		 UInt16 *frameBuffer = buffer.mData;
		 for (int j = 0; j < inNumberFrames; j++) {
		 frameBuffer[j] = rand();
		 }
		 */
		
	}
	
    return noErr;
}

@implementation IosAudioController

@synthesize audioUnit, tempBuffer;

/**
 Initialize the audioUnit and allocate our own temporary buffer.
 The temporary buffer will hold the latest data coming in from the microphone,
 and will be copied to the output when this is requested.
 */
- (id) init {
	self = [super init];
	
	OSStatus status;
	
	// Describe audio component
	AudioComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_RemoteIO;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	
	// Get component
	AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
	
	// Get audio units
	status = AudioComponentInstanceNew(inputComponent, &audioUnit);
	checkStatus(status);
	
	// Enable IO for recording
	UInt32 flag = 1;
	status = AudioUnitSetProperty(audioUnit, 
								  kAudioOutputUnitProperty_EnableIO, 
								  kAudioUnitScope_Input, 
								  kInputBus,
								  &flag, 
								  sizeof(flag));
	checkStatus(status);
	
	// Enable IO for playback
	status = AudioUnitSetProperty(audioUnit, 
								  kAudioOutputUnitProperty_EnableIO, 
								  kAudioUnitScope_Output, 
								  kOutputBus,
								  &flag, 
								  sizeof(flag));
	checkStatus(status);
	
	// Describe format
	AudioStreamBasicDescription audioFormat;
	audioFormat.mSampleRate			= 44100.00;
	audioFormat.mFormatID			= kAudioFormatLinearPCM;
	audioFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	audioFormat.mFramesPerPacket	= 1;
	audioFormat.mChannelsPerFrame	= 1;
	audioFormat.mBitsPerChannel		= 16;
	audioFormat.mBytesPerPacket		= 2;
	audioFormat.mBytesPerFrame		= 2;
	
	// Apply format
	status = AudioUnitSetProperty(audioUnit, 
								  kAudioUnitProperty_StreamFormat, 
								  kAudioUnitScope_Output, 
								  kInputBus, 
								  &audioFormat, 
								  sizeof(audioFormat));
	checkStatus(status);
	status = AudioUnitSetProperty(audioUnit, 
								  kAudioUnitProperty_StreamFormat, 
								  kAudioUnitScope_Input, 
								  kOutputBus, 
								  &audioFormat, 
								  sizeof(audioFormat));
	checkStatus(status);
	
	
	// Set input callback
	AURenderCallbackStruct callbackStruct;
	callbackStruct.inputProc = recordingCallback;
	callbackStruct.inputProcRefCon = self;
	status = AudioUnitSetProperty(audioUnit, 
								  kAudioOutputUnitProperty_SetInputCallback, 
								  kAudioUnitScope_Global, 
								  kInputBus, 
								  &callbackStruct, 
								  sizeof(callbackStruct));
	checkStatus(status);
	
	// Set output callback
	callbackStruct.inputProc = playbackCallback;
	callbackStruct.inputProcRefCon = self;
	status = AudioUnitSetProperty(audioUnit, 
								  kAudioUnitProperty_SetRenderCallback, 
								  kAudioUnitScope_Global, 
								  kOutputBus,
								  &callbackStruct, 
								  sizeof(callbackStruct));
	checkStatus(status);
	
	// Disable buffer allocation for the recorder (optional - do this if we want to pass in our own)
	flag = 0;
	status = AudioUnitSetProperty(audioUnit, 
								  kAudioUnitProperty_ShouldAllocateBuffer,
								  kAudioUnitScope_Output, 
								  kInputBus,
								  &flag, 
								  sizeof(flag));
	
	// Allocate our own buffers (1 channel, 16 bits per sample, thus 16 bits per frame, thus 2 bytes per frame).
	// Practice learns the buffers used contain 512 frames, if this changes it will be fixed in processAudio.
	tempBuffer.mNumberChannels = 1;
	tempBuffer.mDataByteSize = 512 * 2;
	tempBuffer.mData = malloc( 512 * 2 );
	
	// Initialise
	status = AudioUnitInitialize(audioUnit);
	checkStatus(status);
	
	return self;
}

/**
 Start the audioUnit. This means data will be provided from
 the microphone, and requested for feeding to the speakers, by
 use of the provided callbacks.
 */
- (void) start {
	ATA_INFO("Might start the stream ...");
	OSStatus status = AudioOutputUnitStart(audioUnit);
	checkStatus(status);
}

/**
 Stop the audioUnit
 */
- (void) stop {
	ATA_INFO("Might stop the stream ...");
	OSStatus status = AudioOutputUnitStop(audioUnit);
	checkStatus(status);
}

/**
 Change this funtion to decide what is done with incoming
 audio data from the microphone.
 Right now we copy it to our own temporary buffer.
 */
- (void) processAudio: (AudioBufferList*) bufferList{
	AudioBuffer sourceBuffer = bufferList->mBuffers[0];
	
	// fix tempBuffer size if it's the wrong size
	if (tempBuffer.mDataByteSize != sourceBuffer.mDataByteSize) {
		free(tempBuffer.mData);
		tempBuffer.mDataByteSize = sourceBuffer.mDataByteSize;
		tempBuffer.mData = malloc(sourceBuffer.mDataByteSize);
	}
	
	// copy incoming audio data to temporary buffer
	memcpy(tempBuffer.mData, bufferList->mBuffers[0].mData, bufferList->mBuffers[0].mDataByteSize);
}

/**
 Clean up.
 */
- (void) dealloc {
	[super	dealloc];
	AudioUnitUninitialize(audioUnit);
	free(tempBuffer.mData);
}
@end


// ============================================================================================================





namespace airtaudio {
	namespace api {
		class CoreIosPrivate {
		};
	};
};

airtaudio::api::CoreIos::CoreIos(void) :
  m_private(new airtaudio::api::CoreIosPrivate) {
	ATA_INFO("new CoreIos");
	int32_t deviceCount = 2;
	ATA_ERROR("Get count devices : " << 2);
	airtaudio::DeviceInfo tmp;
	// Add default output format :
	tmp.name = "out";
	tmp.sampleRates.push_back(48000);
	tmp.outputChannels = 2;
	tmp.inputChannels = 0;
	tmp.duplexChannels = 0;
	tmp.isDefaultOutput = true;
	tmp.isDefaultInput = false;
	tmp.nativeFormats = SINT16;
	m_devices.push_back(tmp);
	// add default input format:
	tmp.name = "in";
	tmp.sampleRates.push_back(48000);
	tmp.outputChannels = 0;
	tmp.inputChannels = 2;
	tmp.duplexChannels = 0;
	tmp.isDefaultOutput = false;
	tmp.isDefaultInput = true;
	tmp.nativeFormats = SINT16;
	m_devices.push_back(tmp);
	
	ATA_INFO("Create CoreIOs interface (end)");
}

airtaudio::api::CoreIos::~CoreIos(void) {
	ATA_INFO("Destroy CoreIOs interface");
	delete m_private;
	m_private = NULL;
}

uint32_t airtaudio::api::CoreIos::getDeviceCount(void) {
	//ATA_INFO("Get device count:"<< m_devices.size());
	return m_devices.size();
}

airtaudio::DeviceInfo airtaudio::api::CoreIos::getDeviceInfo(uint32_t _device) {
	//ATA_INFO("Get device info ...");
	return m_devices[_device];
}

enum airtaudio::errorType airtaudio::api::CoreIos::closeStream(void) {
	ATA_INFO("Close Stream");
	// Can not close the stream now...
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::CoreIos::startStream(void) {
	ATA_INFO("Start Stream");
	// Can not close the stream now...
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::CoreIos::stopStream(void) {
	ATA_INFO("Stop stream");
	//ewol::Context& tmpContext = ewol::getContext();
	//tmpContext.audioCloseDevice(0);
	// Can not close the stream now...
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::CoreIos::abortStream(void) {
	ATA_INFO("Abort Stream");
	[iosAudio stop];
	//ewol::Context& tmpContext = ewol::getContext();
	//tmpContext.audioCloseDevice(0);
	// Can not close the stream now...
	return airtaudio::errorNone;
}

void airtaudio::api::CoreIos::callBackEvent(void* _data,
                                            int32_t _frameRate) {
	/*
	int32_t doStopStream = 0;
	airtaudio::AirTAudioCallback callback = (airtaudio::AirTAudioCallback) m_stream.callbackInfo.callback;
	double streamTime = getStreamTime();
	airtaudio::streamStatus status = 0;
	if (m_stream.doConvertBuffer[OUTPUT] == true) {
		doStopStream = callback(m_stream.userBuffer[OUTPUT],
		                        NULL,
		                        _frameRate,
		                        streamTime,
		                        status,
		                        m_stream.callbackInfo.userData);
		convertBuffer((char*)_data, (char*)m_stream.userBuffer[OUTPUT], m_stream.convertInfo[OUTPUT]);
	} else {
		doStopStream = callback(_data,
		                        NULL,
		                        _frameRate,
		                        streamTime,
		                        status,
		                        m_stream.callbackInfo.userData);
	}
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	airtaudio::Api::tickStreamTime();
	*/
}

void airtaudio::api::CoreIos::androidCallBackEvent(void* _data,
                                                   int32_t _frameRate,
                                                   void* _userData) {
	/*
	if (_userData == NULL) {
		ATA_INFO("callback event ... NULL pointer");
		return;
	}
	airtaudio::api::Android* myClass = static_cast<airtaudio::api::Android*>(_userData);
	myClass->callBackEvent(_data, _frameRate/2);
	*/
}

bool airtaudio::api::CoreIos::probeDeviceOpen(uint32_t _device,
                                              airtaudio::api::StreamMode _mode,
                                              uint32_t _channels,
                                              uint32_t _firstChannel,
                                              uint32_t _sampleRate,
                                              airtaudio::format _format,
                                              uint32_t *_bufferSize,
                                              airtaudio::StreamOptions *_options) {
	ATA_INFO("Probe : device=" << _device << " channels=" << _channels << " firstChannel=" << _firstChannel << " sampleRate=" << _sampleRate);
	if (_mode != OUTPUT) {
		ATA_ERROR("Can not start a device input or duplex for CoreIos ...");
		return false;
	}
	bool ret = true;
	iosAudio = [[IosAudioController alloc] init];
	[iosAudio start];
	/*
	m_stream.userFormat = _format;
	m_stream.nUserChannels[_mode] = _channels;
	ewol::Context& tmpContext = ewol::getContext();
	if (_format == SINT8) {
		ret = tmpContext.audioOpenDevice(_device, _sampleRate, _channels, 0, androidCallBackEvent, this);
	} else {
		ret = tmpContext.audioOpenDevice(_device, _sampleRate, _channels, 1, androidCallBackEvent, this);
	}
	m_stream.bufferSize = 256;
	m_stream.sampleRate = _sampleRate;
	m_stream.doByteSwap[_mode] = false; // for endienness ...
	
	// TODO : For now, we write it in hard ==> to bu update later ...
	m_stream.deviceFormat[_mode] = SINT16;
	m_stream.nDeviceChannels[_mode] = 2;
	m_stream.deviceInterleaved[_mode] =	true;
	
	m_stream.doConvertBuffer[_mode] = false;
	if (m_stream.userFormat != m_stream.deviceFormat[_mode]) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	if (m_stream.nUserChannels[_mode] < m_stream.nDeviceChannels[_mode]) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	if (    m_stream.userInterleaved != m_stream.deviceInterleaved[_mode]
	     && m_stream.nUserChannels[_mode] > 1) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	if (m_stream.doConvertBuffer[_mode] == true) {
		// Allocate necessary internal buffers.
		uint64_t bufferBytes = m_stream.nUserChannels[_mode] * m_stream.bufferSize * formatBytes(m_stream.userFormat);
		m_stream.userBuffer[_mode] = (char *) calloc(bufferBytes, 1);
		if (m_stream.userBuffer[_mode] == NULL) {
			ATA_ERROR("airtaudio::api::Android::probeDeviceOpen: error allocating user buffer memory.");
		}
		setConvertInfo(_mode, _firstChannel);
	}
	ATA_INFO("device format : " << m_stream.deviceFormat[_mode] << " user format : " << m_stream.userFormat);
	ATA_INFO("device channels : " << m_stream.nDeviceChannels[_mode] << " user channels : " << m_stream.nUserChannels[_mode]);
	ATA_INFO("do convert buffer : " << m_stream.doConvertBuffer[_mode]);
	if (ret == false) {
		ATA_ERROR("Can not open device.");
	}
	*/
	return ret;
}

#endif

