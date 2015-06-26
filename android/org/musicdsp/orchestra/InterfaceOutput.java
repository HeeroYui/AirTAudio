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


public class InterfaceOutput extends Thread implements Constants {
	private int uid = -1;
	private Orchestra ORCHESTRA;
	public static final int SAMPLE_FREQ_44100  = 44100;
	private boolean m_stopAudioThreads = false;
	private AudioTrack m_musicTrack = null;
	
	public InterfaceOutput(int id, Orchestra instance, int idDevice, int freq, int nbChannel, int format) {
		Log.d("InterfaceOutput", "new: output");
		uid = id;
		ORCHESTRA = instance;
		m_stopAudioThreads = false;
	}
	public int getUId() {
		return uid;
	}
	
	public void run() {
		Log.e("InterfaceOutput", "RUN (start)");
		int sampleFreq = SAMPLE_FREQ_44100; //AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);
		int channelConfig = AudioFormat.CHANNEL_CONFIGURATION_STEREO;
		int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
		int nbChannels = 2;
		// we keep the minimum buffer size, otherwite the delay is too big ...
		int bufferSize = AudioTrack.getMinBufferSize(sampleFreq, channelConfig, audioFormat);
		// Create a streaming AudioTrack for music playback
		short[] streamBuffer = new short[bufferSize];
		m_musicTrack = new AudioTrack(AudioManager.STREAM_MUSIC,
		                              SAMPLE_FREQ_44100,
		                              AudioFormat.CHANNEL_CONFIGURATION_STEREO,
		                              AudioFormat.ENCODING_PCM_16BIT,
		                              bufferSize,
		                              AudioTrack.MODE_STREAM);
		m_musicTrack.play();
		m_stopAudioThreads = false;
		//m_musicTrack.setPositionNotificationPeriod(2048);
		
		while (!m_stopAudioThreads) {
			// Fill buffer with PCM data from C++
			ORCHESTRA.playback(uid, streamBuffer, BUFFER_SIZE/nbChannels);
			// Stream PCM data into the music AudioTrack
			m_musicTrack.write(streamBuffer, 0, BUFFER_SIZE);
		}
		
		m_musicTrack.flush();
		m_musicTrack.stop();
		m_musicTrack = null;
		streamBuffer = null;
		Log.e("InterfaceOutput", "RUN (stop)");
	}
	public void Pause() {
		if(m_musicTrack == null) {
			return;
		}
		m_musicTrack.pause();
	}
	public void Resume() {
		if(m_musicTrack == null) {
			return;
		}
		m_musicTrack.play();
	}
	public void AutoStop() {
		if(m_musicTrack == null) {
			return;
		}
		m_stopAudioThreads=true;
	}
}
