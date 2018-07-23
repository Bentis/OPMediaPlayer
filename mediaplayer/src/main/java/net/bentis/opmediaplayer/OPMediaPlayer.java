package net.bentis.opmediaplayer;

import android.app.IntentService;
import android.content.Intent;
import android.support.annotation.Nullable;
import android.util.Log;
import android.content.pm.PackageManager;


public class OPMediaPlayer extends IntentService {
    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }
    static String TAG = "OPMediaPlayer";

    public OPMediaPlayer() {
        super("OPMediaPlayer");
        native_onCreate();
    }

    @Override
    protected void onHandleIntent(@Nullable Intent intent) {
        isStoragePermissionGranted();

        // Gets data from the incoming Intent
        String dataString = intent.getDataString();

        // Do work here, based on the contents of dataString
        Log.v(TAG, "Got string: " + dataString);

        native_onHandleIntent(dataString);
    }

    @Override
    public void onDestroy() {
        Log.v(TAG, "onDestroy");
        super.onDestroy();
    }

    private native void native_onCreate();
    private native void native_onHandleIntent(String filename);

    private  boolean isStoragePermissionGranted() {
        if (checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED) {
            Log.v(TAG,"Permission is granted");
            return true;
        } else {
            Log.v(TAG,"Permission is revoked");
            return false;
        }
    }
}
