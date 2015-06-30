/**
 * @author Edouard DUPIN, Kevin BILLONNEAU
 *
 * @copyright 2015, Edouard DUPIN, all right reserved
 *
 * @license APACHE v2.0 (see license file)
 */

package org.musicdsp.orchestra;

import java.lang.UnsatisfiedLinkError;
import java.lang.RuntimeException;
import android.util.Log;

public class OrchestraNative {
	public <T extends OrchestraManagerCallback> OrchestraNative(T managerInstance) {
		try {
			NNsetJavaManager(managerInstance);
		} catch (java.lang.UnsatisfiedLinkError e) {
			Log.e("Orchestra", "JNI binding not present ...");
			throw new RuntimeException("Orchestra binding not present ...");
		}
		Log.d("Orchestra", "new ...");
	}
	
	public void setManagerRemove() {
		NNsetJavaManagerRemove();
	}
	
	public void playback(int flowId, short[] bufferData, int nbChunk) {
		NNPlayback(flowId, bufferData, nbChunk);
	}
	
	public void record(int flowId, short[] bufferData, int nbChunk) {
		NNRecord(flowId, bufferData, nbChunk);
	}
	
	private native <T extends OrchestraManagerCallback> void NNsetJavaManager(T managerInstance);
	private native void NNsetJavaManagerRemove();
	private native void NNPlayback(int flowId, short[] bufferData, int nbChunk);
	private native void NNRecord(int flowId, short[] bufferData, int nbChunk);
}

