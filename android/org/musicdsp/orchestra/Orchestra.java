/**
 * @author Edouard DUPIN, Kevin BILLONNEAU
 *
 * @copyright 2015, Edouard DUPIN, all right reserved
 *
 * @license APACHE v2.0 (see license file)
 */

package org.musicdsp.orchestra;

import android.util.Log;

public class Orchestra {
	public <T extends ManagerCallback> Orchestra(T managerInstance) {
		NNsetJavaManager(managerInstance);
		Log.d("Orchestra", "new ...");
	}
	
	public void setManagerRemove() {
		NNsetJavaManagerRemove();
	}
	
	private native <T extends ManagerCallback> void NNsetJavaManager(T managerInstance);
	private native void NNsetJavaManagerRemove();
}

