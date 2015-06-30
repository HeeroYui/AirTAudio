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
	public String getDeviceProperty(int idDevice);
	public int openDeviceInput(int idDevice, int sampleRate, int nbChannel, int format);
	public int openDeviceOutput(int idDevice, int sampleRate, int nbChannel, int format);
	public boolean closeDevice(int uniqueID);
	public boolean start(int uniqueID);
	public boolean stop(int uniqueID);
}
