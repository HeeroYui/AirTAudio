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
//import org.musicdsp.orchestra.Orchestra;
//import org.musicdsp.orchestra.InterfaceOutput;
//import org.musicdsp.orchestra.InterfaceInput;

/**
 * @brief Class : 
 *
 */
public class OrchestraManager implements OrchestraManagerCallback, OrchestraConstants {
	private OrchestraNative m_orchestraHandle;
	private int m_uid = 0;
	private Vector<OrchestraInterfaceOutput> m_outputList;
	private Vector<OrchestraInterfaceInput> m_inputList;
	
	public OrchestraManager() {
		// set the java evironement in the C sources :
		m_orchestraHandle = new OrchestraNative(this);
		m_outputList = new Vector<OrchestraInterfaceOutput>();
		m_inputList = new Vector<OrchestraInterfaceInput>();
	}
	
	public int getDeviceCount() {
		Log.e("Manager", "Get device List");
		return 2;
	}
	
	public String getDeviceProperty(int _idDevice) {
		if (_idDevice == 0) {
			return   "{\n"
			       + "	name:'speaker',\n"
			       + "	type:'output',\n"
			       + "	sample-rate:[8000,16000,24000,32000,48000,96000],\n"
			       + "	channels:['front-left','front-right'],\n"
			       + "	format:['int16'],\n"
			       + "	default:true\n"
			       + "}";
		} else if (_idDevice == 1) {
			return   "{\n"
			       + "	name:'microphone',\n"
			       + "	type:'input',\n"
			       + "	sample-rate:[8000,16000,24000,32000,48000,96000],\n"
			       + "	channels:['front-left','front-right'],\n"
			       + "	format:['int16'],\n"
			       + "	default:true\n"
			       + "}";
		} else {
			return "{}";
		}
	}
	
	public int openDeviceOutput(int _idDevice, int _freq, int _nbChannel, int _format) {
		OrchestraInterfaceOutput iface = new OrchestraInterfaceOutput(m_uid, m_orchestraHandle, _idDevice, _freq, _nbChannel, _format);
		m_uid++;
		Log.e("Manager", "Open device Output: " + _idDevice + " with m_uid=" + (m_uid-1));
		if (iface != null) {
			m_outputList.add(iface);
			Log.e("Manager", "Added element count=" + m_outputList.size());
			return m_uid-1;
		}
		return -1;
	}
	
	public int openDeviceInput(int _idDevice, int _freq, int _nbChannel, int _format) {
		OrchestraInterfaceInput iface = new OrchestraInterfaceInput(m_uid, m_orchestraHandle, _idDevice, _freq, _nbChannel, _format);
		m_uid++;
		Log.e("Manager", "Open device Input: " + _idDevice + " with m_uid=" + (m_uid-1));
		if (iface != null) {
			m_inputList.add(iface);
			return m_uid-1;
		}
		return -1;
	}
	
	public boolean closeDevice(int _uniqueID) {
		Log.e("Manager", "Close device : " + _uniqueID);
		if (_uniqueID<0) {
			Log.e("Manager", "Can not Close device with m_uid: " + _uniqueID);
			return false;
		}
		// find the Element with his ID:
		if (m_inputList != null) {
			for (int iii=0; iii<m_inputList.size(); iii++) {
				if (m_inputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				if (m_inputList.get(iii).getUId() == _uniqueID) {
					// find it ...
					m_inputList.remove(iii);
					return true;
				}
			}
		}
		if (m_outputList != null) {
			for (int iii=0; iii<m_outputList.size(); iii++) {
				if (m_outputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				if (m_outputList.get(iii).getUId() == _uniqueID) {
					// find it ...
					m_outputList.remove(iii);
					return true;
				}
			}
		}
		Log.e("Manager", "Can not start device with m_uid: " + _uniqueID + " Element does not exist ...");
		return false;
	}
	
	public boolean start(int _uniqueID) {
		Log.e("Manager", "start device : " + _uniqueID);
		if (_uniqueID<0) {
			Log.e("Manager", "Can not start device with m_uid: " + _uniqueID);
			return false;
		}
		// find the Element with his ID:
		if (m_inputList != null) {
			for (int iii=0; iii<m_inputList.size(); iii++) {
				if (m_inputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				if (m_inputList.get(iii).getUId() == _uniqueID) {
					// find it ...
					m_inputList.get(iii).autoStart();
					return true;
				}
			}
		}
		if (m_outputList != null) {
			for (int iii=0; iii<m_outputList.size(); iii++) {
				if (m_outputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				if (m_outputList.get(iii).getUId() == _uniqueID) {
					// find it ...
					m_outputList.get(iii).autoStart();
					return true;
				}
			}
		}
		Log.e("Manager", "Can not start device with UID: " + _uniqueID + " Element does not exist ...");
		return false;
	}
	
	public boolean stop(int _uniqueID) {
		Log.e("Manager", "stop device : " + _uniqueID);
		if (_uniqueID<0) {
			Log.e("Manager", "Can not stop device with UID: " + _uniqueID);
			return false;
		}
		// find the Element with his ID:
		if (m_inputList != null) {
			for (int iii=0; iii<m_inputList.size(); iii++) {
				if (m_inputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				if (m_inputList.get(iii).getUId() == _uniqueID) {
					// find it ...
					m_inputList.get(iii).autoStop();
					return true;
				}
			}
		}
		if (m_outputList != null) {
			for (int iii=0; iii<m_outputList.size(); iii++) {
				if (m_outputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				if (m_outputList.get(iii).getUId() == _uniqueID) {
					// find it ...
					m_outputList.get(iii).autoStop();
					return true;
				}
			}
		}
		Log.e("Manager", "Can not stop device with UID: " + _uniqueID + " Element does not exist ...");
		return false;
	}
	public void onCreate() {
		Log.w("Manager", "onCreate ...");
		// nothing to do ...
	}
	public void onStart() {
		Log.w("Manager", "onStart ...");
		// nothing to do ...
	}
	public void onRestart() {
		Log.w("Manager", "onRestart ...");
		// nothing to do ...
	}
	public void onResume() {
		Log.w("Manager", "onResume ...");
		// find the Element with his ID:
		if (m_inputList != null) {
			for (int iii=0; iii<m_inputList.size(); iii++) {
				if (m_inputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				m_inputList.get(iii).activityResume();
			}
		}
		if (m_outputList != null) {
			for (int iii=0; iii<m_outputList.size(); iii++) {
				if (m_outputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				m_outputList.get(iii).activityResume();
			}
		}
	}
	public void onPause() {
		Log.w("Manager", "onPause ...");
		// find the Element with his ID:
		if (m_inputList != null) {
			for (int iii=0; iii<m_inputList.size(); iii++) {
				if (m_inputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				m_inputList.get(iii).activityPause();
			}
		}
		if (m_outputList != null) {
			for (int iii=0; iii<m_outputList.size(); iii++) {
				if (m_outputList.get(iii) == null) {
					Log.e("Manager", "Null input element: " + iii);
					continue;
				}
				m_outputList.get(iii).activityPause();
			}
		}
	}
	public void onStop() {
		Log.w("Manager", "onStop ...");
	}
	public void onDestroy() {
		Log.w("Manager", "onDestroy ...");
	}
}

