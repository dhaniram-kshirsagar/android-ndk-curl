/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.example.jnicurl;

import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.TextView;

import android.util.Log;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
/*
 * Simple Java UI to trigger jni function.
 */
public class MainActivity extends AppCompatActivity {
    private String TAG = MainActivity.this.getClass().getName();

    private ThreadPoolExecutor executor = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TextView tv = new TextView(this);
        tv.setText( "DUMMY TEXT" );
        setContentView(tv);
    }

    @Override
    protected void onPostCreate(@Nullable Bundle savedInstanceState) {
        super.onPostCreate(savedInstanceState);
        executor=new ThreadPoolExecutor(1,1,1, TimeUnit.MINUTES,new ArrayBlockingQueue<Runnable>(1,true),new ThreadPoolExecutor.CallerRunsPolicy());
        //run that in separate thread to avoid ui blocking
        executor.execute(new Runnable() {

            @Override
            public void run() {

                stringFromJNI();
                Log.i(TAG, "http request from ndk");
            };
        });
    }

    public native String  stringFromJNI();
    static {
        System.loadLibrary("jni-curl-lib");
    }

}
