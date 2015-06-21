/**
 * @author Edouard DUPIN
 *
 * @copyright 2015, Edouard DUPIN, all right reserved
 *
 * @license APACHE v2.0 (see license file)
 */

package org.musicdsp.orchestra;

import android.util.Log;

//import org.musicdsp.orchestra.Constants;
//import org.musicdsp.orchestra.ManagerCallback;
import org.musicdsp.orchestra.Orchestra;

/**
 * @brief Class : 
 *
 */
public class Manager implements ManagerCallback, Constants {
	private Orchestra orchestraHandle;
	
	public Manager() {
		// set the java evironement in the C sources :
		orchestraHandle = new Orchestra(this);
	}
	
	public int getDeviceCount() {
		Log.e("Manager", "Get device List");
		return 1;
	}
	
	public String getDeviceProperty(int idDevice) {
		if (idDevice == 0) {
			return "speaker:out:8000,16000,24000,32000,48000,96000:2:int16";
		} else {
			return "::::";
		}
	}
	
	public boolean openDevice(int idDevice, int freq, int nbChannel, int format) {
		/*
		if (idDevice == 0) {
			mAudioStarted = true;
			mAudioThread = new Thread(mStreams);
			if (mAudioThread != null) {
				mAudioThread.start();
				return true;
			}
			return false;
		} else {
			Log.e("Manager", "can not open : error unknow device ...");
			return false;
		}
		*/
		return false;
	}
	
	public boolean closeDevice(int idDevice) {
		/*
		if (idDevice == 0) {
			if (mAudioThread != null) {
				// request audio stop
				mStreams.AutoStop();
				// wait the thread ended ...
				try {
					mAudioThread.join();
				} catch(InterruptedException e) { }
				mAudioThread = null;
			}
			mAudioStarted = false;
			return true;
		} else {
			Log.e("Manager", "can not close : error unknow device ...");
			return false;
		}
		*/
		return false;
	}
}

