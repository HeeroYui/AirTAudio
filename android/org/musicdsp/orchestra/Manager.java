/**
 * @author Edouard DUPIN
 *
 * @copyright 2015, Edouard DUPIN, all right reserved
 *
 * @license APACHE v2.0 (see license file)
 */

package org.musicdsp.orchestra;

import android.util.Log;
import java.util.Vector;

//import org.musicdsp.orchestra.Constants;
//import org.musicdsp.orchestra.ManagerCallback;
import org.musicdsp.orchestra.Orchestra;
import org.musicdsp.orchestra.InterfaceOutput;
import org.musicdsp.orchestra.InterfaceInput;

/**
 * @brief Class : 
 *
 */
public class Manager implements ManagerCallback, Constants {
	private Orchestra orchestraHandle;
	private int uid = 0;
	private Vector<InterfaceOutput> outputList;
	private Vector<InterfaceInput> inputList;
	
	public Manager() {
		// set the java evironement in the C sources :
		orchestraHandle = new Orchestra(this);
		outputList = new Vector<InterfaceOutput>();
	}
	
	public int getDeviceCount() {
		Log.e("Manager", "Get device List");
		return 1;
	}
	
	public String getDeviceProperty(int idDevice) {
		if (idDevice == 0) {
			return   "{\n"
			       + "	name:'speaker',\n"
			       + "	type:'output',\n"
			       + "	sample-rate:[8000,16000,24000,32000,48000,96000],\n"
			       + "	channels:['front-left','front-right'],\n"
			       + "	format:['int16'],\n"
			       + "	default:true\n"
			       + "}";
		} else {
			return "{}";
		}
	}
	
	public boolean openDevice(int idDevice, int freq, int nbChannel, int format) {
		InterfaceOutput iface = new InterfaceOutput(uid, orchestraHandle, idDevice, freq, nbChannel, format);
		uid++;
		if (iface != null) {
			outputList.add(iface);
		}
		return false;
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

