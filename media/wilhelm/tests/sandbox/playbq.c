/*
 * Copyright (C) 2010 The Android Open Source Project
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

// Play an audio file using buffer queue

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SLES/OpenSLES.h>
#ifdef ANDROID
#include "sndfile.h"
#else
#include <sndfile.h>
#endif

unsigned numBuffers = 2;
int framesPerBuffer = 512;
SNDFILE *sndfile;
SF_INFO sfinfo;
unsigned which; // which buffer to use next
SLboolean eof;  // whether we have hit EOF on input yet
short *buffers;
SLuint32 byteOrder; // desired to use for PCM buffers
SLuint32 nativeByteOrder;   // of platform
SLuint32 bitsPerSample = 16;

// swap adjacent bytes; this would normally be in <unistd.h> but is missing here
static void swab(const void *from, void *to, ssize_t n)
{
    // from and to as char pointers
    const char *from_ch = (const char *) from;
    char *to_ch = (char *) to;
    // note that we don't swap the last odd byte
    while (n >= 2) {
        to_ch[0] = from_ch[1];
        to_ch[1] = from_ch[0];
        to_ch += 2;
        from_ch += 2;
        n -= 2;
    }
}

// squeeze 16-bit signed PCM samples down to 8-bit unsigned PCM samples by truncation; no dithering
static void squeeze(const short *from, unsigned char *to, ssize_t n)
{
    // note that we don't squeeze the last odd byte
    while (n >= 2) {
        *to++ = (*from++ + 32768) >> 8;
        n -= 2;
    }
}

// This callback is called each time a buffer finishes playing

static void callback(SLBufferQueueItf bufq, void *param)
{
    assert(NULL == param);
    if (!eof) {
        short *buffer = &buffers[framesPerBuffer * sfinfo.channels * which];
        sf_count_t count;
        count = sf_readf_short(sndfile, buffer, (sf_count_t) framesPerBuffer);
        if (0 >= count) {
            eof = SL_BOOLEAN_TRUE;
        } else {
            SLuint32 nbytes = count * sfinfo.channels * sizeof(short);
            if (byteOrder != nativeByteOrder) {
                swab(buffer, buffer, nbytes);
            }
            if (bitsPerSample == 8) {
                squeeze(buffer, (unsigned char *) buffer, nbytes);
                nbytes /= 2;
            }
            SLresult result = (*bufq)->Enqueue(bufq, buffer, nbytes);
            assert(SL_RESULT_SUCCESS == result);
            if (++which >= numBuffers)
                which = 0;
        }
    }
}

int main(int argc, char **argv)
{
    // Determine the native byte order (SL_BYTEORDER_NATIVE not available until 1.1)
    union {
        short s;
        char c[2];
    } u;
    u.s = 0x1234;
    if (u.c[0] == 0x34) {
        nativeByteOrder = SL_BYTEORDER_LITTLEENDIAN;
    } else if (u.c[0] == 0x12) {
        nativeByteOrder = SL_BYTEORDER_BIGENDIAN;
    } else {
        fprintf(stderr, "Unable to determine native byte order\n");
        return EXIT_FAILURE;
    }
    byteOrder = nativeByteOrder;

    SLboolean enableReverb = SL_BOOLEAN_FALSE;
    SLpermille rate = 1000;

    // process command-line options
    int i;
    for (i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (arg[0] != '-')
            break;
        if (!strcmp(arg, "-b")) {
            byteOrder = SL_BYTEORDER_BIGENDIAN;
        } else if (!strcmp(arg, "-l")) {
            byteOrder = SL_BYTEORDER_LITTLEENDIAN;
        } else if (!strcmp(arg, "-8")) {
            bitsPerSample = 8;
        } else if (!strncmp(arg, "-f", 2)) {
            framesPerBuffer = atoi(&arg[2]);
        } else if (!strncmp(arg, "-n", 2)) {
            numBuffers = atoi(&arg[2]);
        } else if (!strncmp(arg, "-p", 2)) {
            rate = atoi(&arg[2]);
        } else if (!strcmp(arg, "-r")) {
            enableReverb = SL_BOOLEAN_TRUE;
        } else {
            fprintf(stderr, "option %s ignored\n", arg);
        }
    }

    if (argc - i != 1) {
        fprintf(stderr, "usage: [-b/l] [-8] [-f#] [-n#] [-p#] [-r] %s filename\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[i];
    //memset(&sfinfo, 0, sizeof(SF_INFO));
    sfinfo.format = 0;
    sndfile = sf_open(filename, SFM_READ, &sfinfo);
    if (NULL == sndfile) {
        perror(filename);
        return EXIT_FAILURE;
    }

    // verify the file format
    switch (sfinfo.channels) {
    case 1:
    case 2:
        break;
    default:
        fprintf(stderr, "unsupported channel count %d\n", sfinfo.channels);
        break;
    }
    switch (sfinfo.samplerate) {
    case  8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        fprintf(stderr, "unsupported sample rate %d\n", sfinfo.samplerate);
        break;
    }
    switch (sfinfo.format & SF_FORMAT_TYPEMASK) {
    case SF_FORMAT_WAV:
        break;
    default:
        fprintf(stderr, "unsupported format type 0x%x\n", sfinfo.format & SF_FORMAT_TYPEMASK);
        break;
    }
    switch (sfinfo.format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_16:
    case SF_FORMAT_PCM_U8:
    case SF_FORMAT_ULAW:
    case SF_FORMAT_ALAW:
    case SF_FORMAT_IMA_ADPCM:
        break;
    default:
        fprintf(stderr, "unsupported sub-format 0x%x\n", sfinfo.format & SF_FORMAT_SUBMASK);
        break;
    }

    buffers = (short *) malloc(framesPerBuffer * sfinfo.channels * sizeof(short) * numBuffers);

    // create engine
    SLresult result;
    SLObjectItf engineObject;
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    SLEngineItf engineEngine;
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);

    // create output mix
    SLObjectItf outputMixObject;
    SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    SLboolean req[1] = {SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, enableReverb ? 1 : 0,
            ids, req);
    assert(SL_RESULT_SUCCESS == result);
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    // configure environmental reverb on output mix
    SLEnvironmentalReverbItf mixEnvironmentalReverb = NULL;
    if (enableReverb) {
        result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                &mixEnvironmentalReverb);
        assert(SL_RESULT_SUCCESS == result);
        SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;
        result = (*mixEnvironmentalReverb)->SetEnvironmentalReverbProperties(mixEnvironmentalReverb,
                &settings);
        assert(SL_RESULT_SUCCESS == result);
    }

    // configure audio source
    SLDataLocator_BufferQueue loc_bufq;
    loc_bufq.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
    loc_bufq.numBuffers = numBuffers;
    SLDataFormat_PCM format_pcm;
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = sfinfo.channels;
    format_pcm.samplesPerSec = sfinfo.samplerate * 1000;
    format_pcm.bitsPerSample = bitsPerSample;
    format_pcm.containerSize = format_pcm.bitsPerSample;
    format_pcm.channelMask = 1 == format_pcm.numChannels ? SL_SPEAKER_FRONT_CENTER :
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    format_pcm.endianness = byteOrder;
    SLDataSource audioSrc;
    audioSrc.pLocator = &loc_bufq;
    audioSrc.pFormat = &format_pcm;

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix;
    loc_outmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    loc_outmix.outputMix = outputMixObject;
    SLDataSink audioSnk;
    audioSnk.pLocator = &loc_outmix;
    audioSnk.pFormat = NULL;

    // create audio player
    SLInterfaceID ids2[3] = {SL_IID_BUFFERQUEUE, SL_IID_PLAYBACKRATE, SL_IID_EFFECTSEND};
    SLboolean req2[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    SLObjectItf playerObject;
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &playerObject, &audioSrc,
            &audioSnk, enableReverb ? 3 : 2, ids2, req2);
    assert(SL_RESULT_SUCCESS == result);

    // realize the player
    result = (*playerObject)->Realize(playerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    // get the effect send interface and enable effect send reverb for this player
    if (enableReverb) {
        SLEffectSendItf playerEffectSend;
        result = (*playerObject)->GetInterface(playerObject, SL_IID_EFFECTSEND, &playerEffectSend);
        assert(SL_RESULT_SUCCESS == result);
        result = (*playerEffectSend)->EnableEffectSend(playerEffectSend, mixEnvironmentalReverb,
                SL_BOOLEAN_TRUE, (SLmillibel) 0);
        assert(SL_RESULT_SUCCESS == result);
    }

    // get the playback rate interface and configure the rate
    SLPlaybackRateItf playerPlaybackRate;
    result = (*playerObject)->GetInterface(playerObject, SL_IID_PLAYBACKRATE, &playerPlaybackRate);
    assert(SL_RESULT_SUCCESS == result);
    SLpermille defaultRate;
    result = (*playerPlaybackRate)->GetRate(playerPlaybackRate, &defaultRate);
    assert(SL_RESULT_SUCCESS == result);
    SLuint32 defaultProperties;
    result = (*playerPlaybackRate)->GetProperties(playerPlaybackRate, &defaultProperties);
    assert(SL_RESULT_SUCCESS == result);
    printf("default playback rate %d per mille, properties 0x%x\n", defaultRate, defaultProperties);
    if (rate != defaultRate) {
        result = (*playerPlaybackRate)->SetRate(playerPlaybackRate, rate);
        if (SL_RESULT_FEATURE_UNSUPPORTED == result) {
            fprintf(stderr, "playback rate %d is unsupported\n", rate);
        } else if (SL_RESULT_PARAMETER_INVALID == result) {
            fprintf(stderr, "playback rate %d is invalid", rate);
        } else {
            assert(SL_RESULT_SUCCESS == result);
        }
    }

    // get the play interface
    SLPlayItf playerPlay;
    result = (*playerObject)->GetInterface(playerObject, SL_IID_PLAY, &playerPlay);
    assert(SL_RESULT_SUCCESS == result);

    // get the buffer queue interface
    SLBufferQueueItf playerBufferQueue;
    result = (*playerObject)->GetInterface(playerObject, SL_IID_BUFFERQUEUE,
            &playerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);

    // loop until EOF or no more buffers
    for (which = 0; which < numBuffers; ++which) {
        short *buffer = &buffers[framesPerBuffer * sfinfo.channels * which];
        sf_count_t frames = framesPerBuffer;
        sf_count_t count;
        count = sf_readf_short(sndfile, buffer, frames);
        if (0 >= count) {
            eof = SL_BOOLEAN_TRUE;
            break;
        }

        // enqueue a buffer
        SLuint32 nbytes = count * sfinfo.channels * sizeof(short);
        if (byteOrder != nativeByteOrder) {
            swab(buffer, buffer, nbytes);
        }
        if (bitsPerSample == 8) {
            squeeze(buffer, (unsigned char *) buffer, nbytes);
            nbytes /= 2;
        }
        result = (*playerBufferQueue)->Enqueue(playerBufferQueue, buffer, nbytes);
        assert(SL_RESULT_SUCCESS == result);
    }
    if (which >= numBuffers)
        which = 0;

    // register a callback on the buffer queue
    result = (*playerBufferQueue)->RegisterCallback(playerBufferQueue, callback, NULL);
    assert(SL_RESULT_SUCCESS == result);

    // set the player's state to playing
    result = (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);

    // wait until the buffer queue is empty
    SLBufferQueueState bufqstate;
    for (;;) {
        result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufqstate);
        assert(SL_RESULT_SUCCESS == result);
        if (0 >= bufqstate.count) {
            break;
        }
        sleep(1);
    }

    // destroy audio player
    (*playerObject)->Destroy(playerObject);

    // destroy output mix
    (*outputMixObject)->Destroy(outputMixObject);

    // destroy engine
    (*engineObject)->Destroy(engineObject);

    return EXIT_SUCCESS;
}
