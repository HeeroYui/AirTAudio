/**
 * @author Edouard DUPIN, Kevin BILLONNEAU
 *
 * @copyright 2015, Edouard DUPIN, all right reserved
 *
 * @license APACHE v2.0 (see license file)
 */
package org.musicdsp.orchestra;

import android.media.AudioTrack;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.util.Log;


public class OrchestraInterfaceOutput extends Thread implements OrchestraConstants {
	private int m_uid = -1;
	private OrchestraNative m_orchestraNativeHandle;
	private boolean m_stop = false;
	private boolean m_suspend = false;
	private AudioTrack m_audio = null;
	
	public OrchestraInterfaceOutput(int id, OrchestraNative instance, int idDevice, int freq, int nbChannel, int format) {
		Log.d("InterfaceOutput", "new: output");
		m_uid = id;
		m_orchestraNativeHandle = instance;
		m_stop = false;
	}
	public int getUId() {
		return m_uid;
	}
	
	public void run() {
		Log.e("InterfaceOutput", "RUN (start)");
		int sampleFreq = 48000; //AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);
		int channelConfig = AudioFormat.CHANNEL_CONFIGURATION_STEREO;
		int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
		int nbChannels = 2;
		// we keep the minimum buffer size, otherwite the delay is too big ...
		int bufferSize = AudioTrack.getMinBufferSize(sampleFreq, channelConfig, audioFormat);
		// Create a streaming AudioTrack for music playback
		short[] streamBuffer = new short[bufferSize];
		m_audio = new AudioTrack(AudioManager.STREAM_MUSIC,
		                         sampleFreq,
		                         AudioFormat.CHANNEL_CONFIGURATION_STEREO,
		                         AudioFormat.ENCODING_PCM_16BIT,
		                         bufferSize,
		                         AudioTrack.MODE_STREAM);
		m_audio.play();
		//m_audio.setPositionNotificationPeriod(2048);
		
		while (m_stop == false) {
			// Fill buffer with PCM data from C++
			m_orchestraNativeHandle.playback(m_uid, streamBuffer, BUFFER_SIZE/nbChannels);
			// Stream PCM data into the music AudioTrack
			m_audio.write(streamBuffer, 0, BUFFER_SIZE);
		}
		
		m_audio.flush();
		m_audio.stop();
		m_audio = null;
		streamBuffer = null;
		Log.e("InterfaceOutput", "RUN (stop)");
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
		m_audio.play();
	}
	public void activityPause() {
		if(m_audio == null) {
			return;
		}
		m_audio.pause();
	}
}
