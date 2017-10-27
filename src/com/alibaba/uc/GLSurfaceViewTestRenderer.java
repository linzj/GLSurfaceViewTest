/*
 * Copyright (C) 2009 The Android Open Source Project
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
package com.alibaba.uc;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import javax.microedition.khronos.opengles.GL10;
import javax.microedition.khronos.egl.EGLConfig;
import android.opengl.GLES20;
import android.util.Log;
import android.graphics.SurfaceTexture;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Shader;
import android.graphics.LinearGradient;
import android.view.Surface;
import java.nio.FloatBuffer;
import java.nio.ByteOrder;
// import java.nio.Buffer;
import java.nio.ByteBuffer;

public class GLSurfaceViewTestRenderer implements GLSurfaceView.Renderer {
    private int mProgramHandle;
    private int maPositionHandle;
    private int maTextureHandle;
    private int muSamplerHandle;
    private int mTextureHandle;
    private final String TAG = "LINZJ";

    public void onDrawFrame(GL10 gl) {
        GLES20.glClearColor(0.0f, 1.0f, 0.0f, 0.0f);
        GLES20.glClear(GL10.GL_COLOR_BUFFER_BIT);
        GLES20.glUseProgram(mProgramHandle);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_FAN, 0, 4);
        checkGlError("onDrawFrame");
    }

    public void onSurfaceChanged(GL10 gl, int width, int height) {
        GLES20.glViewport(0, 0, width, height);
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        // Do nothing.
        mProgramHandle = createProgram(mVertexShader, mFragmentShader);
        if (mProgramHandle == 0) {
            throw new RuntimeException("failed to create program");
        }

        maPositionHandle = GLES20.glGetAttribLocation(mProgramHandle, "aPosition");
        checkGlError("glGetAttribLocation aPosition");
        if (maPositionHandle == -1) {
            throw new RuntimeException("Could not get attrib location for aPosition");
        }
        maTextureHandle = GLES20.glGetAttribLocation(mProgramHandle, "aTextureCoord");
        checkGlError("glGetAttribLocation aTextureCoord");
        if (maTextureHandle == -1) {
            throw new RuntimeException("Could not get attrib location for aTextureCoord");
        }

        muSamplerHandle = GLES20.glGetUniformLocation(mProgramHandle, "sTexture");
        checkGlError("glGetUniformLocation sTexture");
        if (muSamplerHandle == -1) {
            throw new RuntimeException("Could not get uniform location for muSamplerHandle");
        }

        GLES20.glEnableVertexAttribArray(maTextureHandle);
        GLES20.glEnableVertexAttribArray(maPositionHandle);
        FloatBuffer fbVertex = ByteBuffer.allocateDirect(4 * 3 * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        FloatBuffer fbTex = ByteBuffer.allocateDirect(4 * 2 * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        fbVertex.put(new float[] {
           -1.0f, 1.0f, 0.0f,
           -1.0f, -1.0f, 0.0f,
           1.0f, -1.0f, 0.0f,
           1.0f, 1.0f, 0.0f,
        }).position(0);
        // fbVertex.rewind();
        fbTex.put(new float[] {
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
        }).position(0);
        GLES20.glVertexAttribPointer(maPositionHandle, 3, GLES20.GL_FLOAT, false, 0, fbVertex);
        GLES20.glVertexAttribPointer(maTextureHandle, 2, GLES20.GL_FLOAT, false, 0, fbTex);
        checkGlError("bind vao");
        int[] textures = new int[1];
        GLES20.glGenTextures(1, textures, 0);
        checkGlError("gen textures");
        mTextureHandle = textures[0];
        SurfaceTexture st = new SurfaceTexture(mTextureHandle);
        st.setDefaultBufferSize(128, 128);
        Surface sf = new Surface(st);
        Canvas c = sf.lockCanvas(new Rect(0, 0, 128, 128));
        Paint gradientPaint = new Paint();
        gradientPaint.setShader(new LinearGradient(0, 0, 128, 128, Color.BLACK, Color.WHITE, Shader.TileMode.MIRROR));
        c.drawRect(0.0f, 0.0f, 128.0f, 128.0f , gradientPaint);
        Paint textPaint = new Paint();
        textPaint.setColor(Color.RED);
        textPaint.setStrokeWidth(3);
        textPaint.setTextSize(40.0f);
        textPaint.setTextAlign(Paint.Align.LEFT);
        c.drawText("A", 32, 32, textPaint);
        sf.unlockCanvasAndPost(c);
        st.updateTexImage();
        st.release();
    }

    private int loadShader(int shaderType, String source) {
        int shader = GLES20.glCreateShader(shaderType);
        if (shader != 0) {
            GLES20.glShaderSource(shader, source);
            GLES20.glCompileShader(shader);
            int[] compiled = new int[1];
            GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compiled, 0);
            if (compiled[0] == 0) {
                Log.e(TAG, "Could not compile shader " + shaderType + ":");
                Log.e(TAG, GLES20.glGetShaderInfoLog(shader));
                GLES20.glDeleteShader(shader);
                shader = 0;
            }
        }
        return shader;
    }

    private void checkGlError(String op) {
        int error;
        while ((error = GLES20.glGetError()) != GLES20.GL_NO_ERROR) {
            Log.e(TAG, op + ": glError " + error);
            throw new RuntimeException(op + ": glError " + error);
        }
    }

    private int createProgram(String vertexSource, String fragmentSource) {
        int vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, vertexSource);
        if (vertexShader == 0) {
            return 0;
        }

        int pixelShader = loadShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource);
        if (pixelShader == 0) {
            return 0;
        }

        int program = GLES20.glCreateProgram();
        if (program != 0) {
            GLES20.glAttachShader(program, vertexShader);
            checkGlError("glAttachShader");
            GLES20.glAttachShader(program, pixelShader);
            checkGlError("glAttachShader");
            GLES20.glLinkProgram(program);
            int[] linkStatus = new int[1];
            GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0);
            if (linkStatus[0] != GLES20.GL_TRUE) {
                Log.e(TAG, "Could not link program: ");
                Log.e(TAG, GLES20.glGetProgramInfoLog(program));
                GLES20.glDeleteProgram(program);
                program = 0;
            }
        }
        return program;
    }

    private final String mVertexShader =
        "attribute vec4 aPosition;\n" +
        "attribute vec2 aTextureCoord;\n" +
        "varying vec2 vTextureCoord;\n" +
        "void main() {\n" +
        "  gl_Position = aPosition;\n" +
        "  vTextureCoord = aTextureCoord;\n" +
        "}\n";

    private final String mFragmentShader =
        "#extension GL_OES_EGL_image_external : require\n" +
        "precision mediump float;\n" +
        "varying vec2 vTextureCoord;\n" +
        "uniform samplerExternalOES sTexture;\n" +
        "void main() {\n" +
        "  gl_FragColor = texture2D(sTexture, vTextureCoord);;\n" +
        "}\n";
} 
