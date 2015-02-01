/*
*
*	Client to connect to jackd and spit out audio
*
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <jack/jack.h>
#include <semaphore.h>
#include "jack_client.h"

jack_port_t *in_port;
jack_port_t *out_port;
jack_client_t *client;

#define BUFFER_LEN		1024
#define BUFFER_BYTES	BUFFER_LEN * sizeof(float)
#define SAMPLE_RATE		44100
#define SINE_WAVE_LEN	BUFFER_LEN
#define PI				3.14159265

typedef struct
{
	int index;			//position of index in sample
	int sample_l;		//length of buffer
	float *sample_buf;	//sample buffer
} mp3_sample;

float sine_wave_buf[SINE_WAVE_LEN];
//sample library
mp3_sample global_samples;

//samples being played
mp3_sample playing_samples;
int playing_samples_l = 0;
sem_t *playing_samples_sem;

void output_sample(mp3_sample *smp, float *buf){
	int index = smp->index;
	int size = smp->sample_l;
	float *sample_buf = smp->sample_buf;
	int play_len = BUFFER_LEN;
	if(size - index < BUFFER_LEN){
		play_len = size - index;
	}
	//printf("i:%d, l:%d\n", index, play_len);
	for (int i = 0; i < play_len; ++i)
	{
		buf[i] += sample_buf[index++];
		if(index >= size){
			index -= size;
		}
	}
	smp->index = index;
}

//Process the audio frames
int process_frames(jack_nframes_t nframes, void *arg){
	//get buffer
	float *out = (float*) jack_port_get_buffer (out_port, nframes);
	//reset
	memset(out, 0, BUFFER_BYTES);
	//play samples
	for (int i = 0; i < playing_samples_l; ++i)
	{
		if(playing_samples[i].index < playing_samples[i].sample_l){
			output_sample(&playing_samples[i], out);
		}
	}
	return 0;
}

//Shutdown if error
void jack_shutdown(void *arg){
	exit(1);
}

//Creates a mp3 sample object given the path, start and end times
mp3_sample load_mp3_sample(const char* filename, float start, float end){
	mp3_sample smp = {0, 0, NULL};

	//delete if exists
	system("rm -f sample.raw");

	//convert to raw 32-bit float
	char command[200];
	sprintf(command, "avconv -i %s -ac 1 -f f32le sample.raw -loglevel quiet", filename);
	system(command);

	int bytes, size;
	FILE* f = fopen("sample.raw", "rb");
	if(f == NULL){
		printf("File not found\n");
		return smp;
	}
	fseek(f, 0L, SEEK_END);
	size = ftell(f);
	rewind(f);
	float* samples = (float*)malloc(size);
	int pos = 0;
	float val = 0;
	//float max = 0.0;
	while((bytes = fread(&val, sizeof(float), 1, f))){
		samples[pos] = val;
		pos++;
	}
	//copy to struct
	smp.sample_l = size / sizeof(float);
	smp.sample_buf = samples;

	//clean up
	system("rm -f sample.raw");

	return smp;
}

//Main function
void start_pijam()
{
	const char **ports;
	//init sine wave
	for (int i = 0; i < SINE_WAVE_LEN; ++i)
	{
		sine_wave_buf[i] = sin((float)(2 * PI * i) / SINE_WAVE_LEN);
	}
	//init samples
	global_samples = 
	global_samples[0] = load_mp3_sample("samples/funk_d.mp3", 0, 0);
	//connect to jackd
	client = jack_client_open ("pedal", JackNullOption, NULL);
	//set process callback to function above
	jack_set_process_callback (client, process_frames, 0);
	//set shutdown callback to function above
	jack_on_shutdown (client, jack_shutdown, 0);
	//in_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    out_port = jack_port_register (client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	// tell the JACK server that we are ready to roll
	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		return;
	}
	//connect to output port
	if ((ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput)) == NULL) {
		fprintf(stderr, "Cannot find any physical playback ports\n");
		exit(1);
	}
	if (jack_connect (client, jack_port_name (out_port), ports[0])) {
		fprintf (stderr, "cannot connect output ports\n");
	}
	free (ports);
	//start up semaphore for playing samples array
	playing_samples_sem = sem_open ("pSem", O_CREAT | O_EXCL, 0644, 0); 
    /* name of semaphore is "pSem", semaphore is reached using this name */
    sem_unlink ("pSem");
}

void stop_pijam(){
	sem_destroy (playing_samples_sem);
	jack_client_close (client);
}

//plays a sample by index
void play_sample(int i){
	sem_wait(playing_samples_sem);
	int i;
	for (i = 0; i < playing_samples_l; ++i)
	{
		if(playing_samples[i].index >= playing_samples[i].sample_l){
			//index is over length, sample is garbaged
			break;
		}
	}
	if(i >= playing_samples_l){
		printf("out of range\n");
	}else{
		playing_samples[i] = global_samples[i];
	}
	sem_post(playing_samples_sem);
}