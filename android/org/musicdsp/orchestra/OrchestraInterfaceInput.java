/**
 * @author Edouard DUPIN, Kevin BILLONNEAU
 *
 * @copyright 2015, Edouard DUPIN, all right reserved
 *
 * @license APACHE v2.0 (see license file)
 */

package org.musicdsp.orchestra;
import android.media.AudioRecord;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;



public class OrchestraInterfaceInput extends Thread implements OrchestraConstants {
	private int m_uid = -1;
	private OrchestraNative m_orchestraNativeHandle;
	public static final int SAMPLE_FREQ_44100 = 44100;
	private boolean m_stop = false;
	private AudioRecord m_audio = null;
	
	public OrchestraInterfaceInput(int id, OrchestraNative instance, int idDevice, int freq, int nbChannel, int format) {
		Log.d("InterfaceInput", "new: output");
		m_uid = id;
		m_orchestraNativeHandle = instance;
		m_stop = false;
	}
	public int getUId() {
		return m_uid;
	}
	
	public void run() {
		Log.e("InterfaceInput", "RUN (start)");
		int sampleFreq = 48000;
		int channelConfig = AudioFormat.CHANNEL_CONFIGURATION_STEREO;
		int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
		int nbChannels = 2;
		// we keep the minimum buffer size, otherwite the delay is too big ...
		int bufferSize = AudioRecord.getMinBufferSize(sampleFreq, channelConfig, audioFormat);
		// Create a streaming AudioTrack for music playback
		short[] streamBuffer = new short[bufferSize];
		m_audio = new AudioRecord(MediaRecorder.AudioSource.MIC,
		                          sampleFreq,
		                          AudioFormat.CHANNEL_CONFIGURATION_STEREO,
		                          AudioFormat.ENCODING_PCM_16BIT,
		                          bufferSize);
		m_audio.startRecording();
		m_stop = false;
		
		while (m_stop == false) {
			// Stream PCM data into the local buffer
			m_audio.read(streamBuffer, 0, BUFFER_SIZE);
			// Send it to C++
			m_orchestraNativeHandle.record(m_uid, streamBuffer, BUFFER_SIZE/nbChannels);
		}
		m_audio.stop();
		m_audio = null;
		streamBuffer = null;
		Log.e("InterfaceInput", "RUN (stop)");
	}
	public void autoStop() {
		if(m_audio == null) {
			return;
		}
		m_stop=true;
	}
	public void activityResume() {
		if(m_audio == null) {
			return;
		}
		
	}
	public void activityPause() {
		if(m_audio == null) {
			return;
		}
		
	}
}
