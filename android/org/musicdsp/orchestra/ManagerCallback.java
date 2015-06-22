/**
 * @author Edouard DUPIN, Kevin BILLONNEAU
 *
 * @copyright 2015, Edouard DUPIN, all right reserved
 *
 * @license APACHE v2.0 (see license file)
 */

package org.musicdsp.orchestra;

public interface ManagerCallback {
	public int getDeviceCount();
	public String getDeviceProperty(int idDevice);
	public boolean openDevice(int idDevice, int sampleRate, int nbChannel, int format);
	public boolean closeDevice(int idDevice);
}
