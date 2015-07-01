/**
 * @author Edouard DUPIN, Kevin BILLONNEAU
 *
 * @copyright 2015, Edouard DUPIN, all right reserved
 *
 * @license APACHE v2.0 (see license file)
 */

package org.musicdsp.orchestra;

public interface OrchestraManagerCallback {
	public int getDeviceCount();
	public String getDeviceProperty(int _idDevice);
	public int openDeviceInput(int _idDevice, int _sampleRate, int _nbChannel, int _format);
	public int openDeviceOutput(int _idDevice, int _sampleRate, int _nbChannel, int _format);
	public boolean closeDevice(int _uniqueID);
	public boolean start(int _uniqueID);
	public boolean stop(int _uniqueID);
}
