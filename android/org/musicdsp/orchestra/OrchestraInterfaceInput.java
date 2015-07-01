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



public class OrchestraInterfaceInput implements Runnable, OrchestraConstants {
	private Thread m_thread = null;
	private int m_uid = -1;
	private OrchestraNative m_orchestraNativeHandle;
	private boolean m_stop = false;
	private boolean m_suspend = false;
	private AudioRecord m_audio = null;
	private int m_sampleRate = 48000;
	private int m_nbChannel = 2;
	private int m_format = 1;
	private int m_bufferSize = BUFFER_SIZE;
	
	public OrchestraInterfaceInput(int _id, OrchestraNative _instance, int _idDevice, int _sampleRate, int _nbChannel, int _format) {
		Log.d("InterfaceInput", "new: Input");
		m_uid = _id;
		m_orchestraNativeHandle = _instance;
		m_stop = false;
		m_suspend = false;
		m_sampleRate = _sampleRate;
		m_nbChannel = _nbChannel;
		m_format = _format;
		m_bufferSize = BUFFER_SIZE * m_nbChannel;
	}
	public int getUId() {
		return m_uid;
	}
	
	public void run() {
		Log.e("InterfaceInput", "RUN (start)");
		int channelConfig = AudioFormat.CHANNEL_CONFIGURATION_STEREO;
		int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
		// we keep the minimum buffer size, otherwite the delay is too big ...
		// TODO : int bufferSize = AudioRecord.getMinBufferSize(m_sampleRate, channelConfig, audioFormat);
		int config = 0;
		if (m_nbChannel == 1) {
			config = AudioFormat.CHANNEL_IN_MONO;
		} else {
			config = AudioFormat.CHANNEL_IN_STEREO;
		}
		// Create a streaming AudioTrack for music playback
		short[] streamBuffer = new short[m_bufferSize];
		m_audio = new AudioRecord(MediaRecorder.AudioSource.MIC,
		                          m_sampleRate,
		                          config,
		                          audioFormat,
		                          m_bufferSize);
		m_audio.startRecording();
		
		while (    m_stop == false
		        && m_suspend == false) {
			// Stream PCM data into the local buffer
			m_audio.read(streamBuffer, 0, m_bufferSize);
			// Send it to C++
			m_orchestraNativeHandle.record(m_uid, streamBuffer, m_bufferSize/m_nbChannel);
		}
		m_audio.stop();
		m_audio = null;
		streamBuffer = null;
		Log.e("InterfaceInput", "RUN (stop)");
	}
	
	public void autoStart() {
		m_stop=false;
		if (m_suspend == false) {
			Log.e("InterfaceInput", "Create thread");
			m_thread = new Thread(this);
			Log.e("InterfaceInput", "start thread");
			m_thread.start();
			Log.e("InterfaceInput", "start thread (done)");
		}
	}
	
	public void autoStop() {
		if(m_audio == null) {
			return;
		}
		m_stop=true;
		m_thread = null;
		/*
		try {
			super.join();
		} catch(InterruptedException e) { }
		*/
	}
	public void activityResume() {
		m_suspend = false;
		if (m_stop == false) {
			Log.i("InterfaceInput", "Resume audio stream : " + m_uid);
			m_thread = new Thread(this);
			m_thread.start();
		}
	}
	public void activityPause() {
		if(m_audio == null) {
			return;
		}
		m_suspend = true;
		Log.i("InterfaceInput", "Pause audio stream : " + m_uid);
		m_thread = null;
	}
}
