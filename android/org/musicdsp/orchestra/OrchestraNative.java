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
	public <T extends OrchestraManagerCallback> OrchestraNative(T _managerInstance) {
		try {
			NNsetJavaManager(_managerInstance);
		} catch (java.lang.UnsatisfiedLinkError e) {
			Log.e("Orchestra", "JNI binding not present ...");
			throw new RuntimeException("Orchestra binding not present ...");
		}
		Log.d("Orchestra", "new ...");
	}
	
	public void setManagerRemove() {
		NNsetJavaManagerRemove();
	}
	
	public void playback(int _flowId, short[] _bufferData, int _nbChunk) {
		NNPlayback(_flowId, _bufferData, _nbChunk);
	}
	
	public void record(int _flowId, short[] _bufferData, int _nbChunk) {
		NNRecord(_flowId, _bufferData, _nbChunk);
	}
	
	private native <T extends OrchestraManagerCallback> void NNsetJavaManager(T _managerInstance);
	private native void NNsetJavaManagerRemove();
	private native void NNPlayback(int _flowId, short[] _bufferData, int _nbChunk);
	private native void NNRecord(int _flowId, short[] _bufferData, int _nbChunk);
}

