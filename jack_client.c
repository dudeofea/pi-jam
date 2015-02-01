/*
*
*	Client to connect to jackd and spit out audio
*
*/
//fix for usleep
#define _BSD_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <jack/jack.h>
#include <semaphore.h>
#include <fcntl.h>
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
	float index;			//position of index in sample
	int sample_l;		//length of buffer
	float speed;
	float *sample_buf;	//sample buffer
} mp3_sample;

float sine_wave_buf[SINE_WAVE_LEN];
//sample library
mp3_sample* global_samples;

//samples being played
mp3_sample* playing_samples;
int playing_samples_l = 0;
int playing_samples_c = 0;
sem_t *playing_samples_sem;

void output_sample(int smp_i, float *buf){
	int play_len = BUFFER_LEN;
	if(playing_samples[smp_i].sample_l - playing_samples[smp_i].index < BUFFER_LEN){
		play_len = playing_samples[smp_i].sample_l - playing_samples[smp_i].index;
	}
	for (int i = 0; i < play_len; ++i)
	{
		buf[i] += playing_samples[smp_i].sample_buf[(int)playing_samples[smp_i].index];
		playing_samples[smp_i].index += playing_samples[smp_i].speed;
		if(playing_samples[smp_i].index >= playing_samples[smp_i].sample_l){
			printf("done\n");
			sem_wait(playing_samples_sem);
			//remove from array
			playing_samples_c--;
			for (int j = smp_i; j < playing_samples_c; ++j)
			{
				//shift
				playing_samples[j] = playing_samples[j+1];
			}
			printf("rem l: %d\n", playing_samples_c);
			sem_post(playing_samples_sem);
			break;
		}
	}
}

//Process the audio frames
int process_frames(jack_nframes_t nframes, void *arg){
	//get buffer
	float *out = (float*) jack_port_get_buffer (out_port, nframes);
	//reset
	memset(out, 0, BUFFER_BYTES);
	//play samples
	for (int i = 0; i < playing_samples_c; ++i)
	{
		output_sample(i, out);
	}
	return 0;
}

//Shutdown if error
void jack_shutdown(void *arg){
	exit(1);
}

//Creates a mp3 sample object given the path, start and end times
mp3_sample load_mp3_sample(const char* filename, float start, float end){
	mp3_sample smp = {0, 0, 0, NULL};
	if(end > 0 && end < start){
		printf("start must be before end\n");
		return smp;
	}

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
	size = ftell(f) / sizeof(float);
	rewind(f);
	int max_bytes = SAMPLE_RATE * (end - start);
	if(end > 0 && max_bytes < size){
		printf("resizing\n");
		size = max_bytes;
	}
	float* samples = (float*)malloc(size * sizeof(float));
	int pos = 0;
	float val = 0;
	//skip to start value (in seconds)
	//printf("start: %d size: %d end: %f\n", (int)(SAMPLE_RATE * start), size, end);
	fseek(f, (int)(SAMPLE_RATE * start * sizeof(float)), SEEK_SET);
	//read bytes
	while((bytes = fread(&val, sizeof(float), 1, f))){
		samples[pos] = val;
		pos++;
		//printf("i:%d\n", pos);
		if(pos >= size){
			break;
		}
	}
	//copy to struct
	smp.sample_l = size;
	smp.speed = 1;
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
	global_samples = (mp3_sample*) malloc(10*sizeof(mp3_sample));
	playing_samples = (mp3_sample*) malloc(10*sizeof(mp3_sample));
	playing_samples_l = 10;
	global_samples[0] = load_mp3_sample("/home/pi/pi-jam/samples/taiko.mp3", 0.0, 0.8);
	global_samples[1] = load_mp3_sample("/home/pi/pi-jam/samples/taiko.mp3", 2.4, 3.2);
	global_samples[2] = load_mp3_sample("/home/pi/pi-jam/samples/funk_d.mp3", 0, 0);
	global_samples[3] = load_mp3_sample("/home/pi/pi-jam/samples/low_d.mp3", 0, 0);
	global_samples[4] = load_mp3_sample("/home/pi/pi-jam/samples/mid_d.mp3", 0, 0);
	global_samples[5] = load_mp3_sample("/home/pi/pi-jam/samples/high_d.mp3", 0, 0);
	global_samples[6] = load_mp3_sample("/home/pi/pi-jam/samples/airhorn.mp3", 0, 0);
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
	playing_samples_sem = sem_open ("pSem", O_CREAT | O_EXCL, 0644, 1); 
    /* name of semaphore is "pSem", semaphore is reached using this name */
    sem_unlink ("pSem");
}

void stop_pijam(){
	free(global_samples);
	free(playing_samples);
	sem_destroy (playing_samples_sem);
	jack_client_close (client);
}

//plays a sample by index
void play_sample(int ins, int pitch_shift){
	usleep(10000);
	sem_wait(playing_samples_sem);
	//load up sample
	playing_samples[playing_samples_c] = global_samples[ins];
	playing_samples[playing_samples_c].speed = pow(2.0, (float)pitch_shift / 12);
	playing_samples_c++;
	//if out of bounds
	printf("add l: %d\n", playing_samples_c);
	if(playing_samples_c >= playing_samples_l){
		printf("out of range\n");
		playing_samples_l *= 2;
		playing_samples = (mp3_sample*)realloc(playing_samples, playing_samples_l * sizeof(mp3_sample));
	}
	sem_post(playing_samples_sem);
}
