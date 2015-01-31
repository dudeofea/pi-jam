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

jack_port_t *in_port;
jack_port_t *out_port;

#define BUFFER_LEN		1024
#define BUFFER_BYTES	BUFFER_LEN * sizeof(float)
#define SAMPLE_RATE		44100
#define SINE_WAVE_LEN	BUFFER_LEN
#define PI				3.14159265

float sine_wave_buf[SINE_WAVE_LEN];

void output_samples(float *out){
	static int sine_i = 0;
	for (int i = 0; i < BUFFER_LEN; ++i)
	{
		out[i] += sine_wave_buf[sine_i];
		sine_i += 6;
		if(sine_i >= BUFFER_LEN){
			sine_i -= BUFFER_LEN;
		}
	}
}

//Process the audio frames
int process_frames(jack_nframes_t nframes, void *arg){
	//get buffer
	float *out = (float*) jack_port_get_buffer (out_port, nframes);
	//reset
	memset(out, 0, BUFFER_BYTES);
	//write out
	output_samples(out);
	return 0;
}

//Shutdown if error
void jack_shutdown(void *arg){
	exit(1);
}

//Main function
int main(int argc, char *argv[])
{
	jack_client_t *client;
	const char **ports;
	//init sine wave
	for (int i = 0; i < SINE_WAVE_LEN; ++i)
	{
		sine_wave_buf[i] = sin((float)(2 * PI * i) / SINE_WAVE_LEN);
	}
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
		return 1;
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
	//wait
	sleep(5);
	//quit the client
	jack_client_close (client);
	exit(0);
}