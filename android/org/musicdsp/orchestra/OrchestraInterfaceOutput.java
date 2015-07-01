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
	private int m_sampleRate = 48000;
	private int m_nbChannel = 2;
	private int m_format = 1;
	private int m_bufferSize = BUFFER_SIZE;
	
	public OrchestraInterfaceOutput(int _id, OrchestraNative _instance, int _idDevice, int _sampleRate, int _nbChannel, int _format) {
		Log.d("InterfaceOutput", "new: output");
		m_uid = _id;
		m_orchestraNativeHandle = _instance;
		m_stop = true;
		m_sampleRate = _sampleRate;
		m_nbChannel = _nbChannel;
		m_format = _format;
		m_bufferSize = BUFFER_SIZE * m_nbChannel;
	}
	public int getUId() {
		return m_uid;
	}
	
	public void run() {
		Log.e("InterfaceOutput", "RUN (start)");
		int channelConfig = AudioFormat.CHANNEL_CONFIGURATION_STEREO;
		int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
		// we keep the minimum buffer size, otherwite the delay is too big ...
		//int bufferSize = AudioTrack.getMinBufferSize(m_sampleRate, channelConfig, audioFormat);
		int config = 0;
		if (m_nbChannel == 1) {
			config = AudioFormat.CHANNEL_OUT_MONO;
		} else if (m_nbChannel == 4) {
			config = AudioFormat.CHANNEL_OUT_QUAD;
		} else {
			config = AudioFormat.CHANNEL_OUT_STEREO;
		}
		// Create a streaming AudioTrack for music playback
		short[] streamBuffer = new short[m_bufferSize];
		m_audio = new AudioTrack(AudioManager.STREAM_MUSIC,
		                         m_sampleRate,
		                         config,
		                         audioFormat,
		                         m_bufferSize,
		                         AudioTrack.MODE_STREAM);
		m_audio.play();
		//m_audio.setPositionNotificationPeriod(2048);
		
		while (m_stop == false) {
			// Fill buffer with PCM data from C++
			m_orchestraNativeHandle.playback(m_uid, streamBuffer, m_bufferSize/m_nbChannel);
			// Stream PCM data into the music AudioTrack
			m_audio.write(streamBuffer, 0, m_bufferSize);
		}
		
		m_audio.flush();
		m_audio.stop();
		m_audio = null;
		streamBuffer = null;
		Log.e("InterfaceOutput", "RUN (stop)");
	}
	public void autoStart() {
		m_stop=false;
		this.start();
	}
	public void autoStop() {
		if(m_audio == null) {
			return;
		}
		m_stop=true;
		try {
			super.join();
		} catch(InterruptedException e) { }
	}
	public void activityResume() {
		if (m_audio != null) {
			Log.i("InterfaceOutput", "Resume audio stream : " + m_uid);
			m_audio.play();
		}
	}
	public void activityPause() {
		if(m_audio == null) {
			return;
		}
		if (m_audio != null) {
			Log.i("InterfaceOutput", "Pause audio stream : " + m_uid);
			m_audio.pause();
		}
	}
}
