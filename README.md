# nausea

This is a simple audio spectrum visualizer.  It works well with the mpd
music player daemon, as well as anything that can play or record audio to stdout.

It was originally inspired by the visualizer
screen of ncmpcpp.  It depends on the fftw3 and curses libraries.

To visualize system audio, you must first make a fifo using mkfifo, and then use
something like aplay or arecord to redirect audio into your fifo:

````
mkfifo /tmp/audio.fifo
aplay > /tmp/audio.fifo
````

You need to add the following to your mpd.conf.  The format is important
because it's the only one supported for now.

    audio_output {
        type "fifo"
        name "Pipe"
        path "/tmp/audio.fifo"
        format "44100:16:2"
    }

Then start the program with:

    $ nausea

Alternatively specify the path of your mpd fifo with:

    $ nausea <path-to-fifo>

To try it out with color support try:

    $ xterm -bg black -fa "Monospace:pixelsize=12" -e ./nausea -c
    
    

## Man Page
### DESCRIPTION
The nausea program performs a discrete fourier transform and plots the spectrogram in real time using curses. It can also just display the audio waveform. The current implementation expects the input stream to be 44.1kHz, 16-bit little endian and 2 channels. The default fifo path is /tmp/audio.fifo.

#### Commandline options
Flag				|Description
----------------|------------------------------------------------------------
−h				|Show usage line.  
−d num			|Choose a visualization using its number from below.  
−c				|Enable color.  
−p				|Enable falling peaks in the spectrum visualization.  
−k				|Keep state in the fountain visualization.  
−l				|Go left in the fountain visualization.  
−b				|Enable bounce mode in the fountain visualization.  

#### Active hotkeys while nausea is running
Key				|Action
----------------|------------------------------------------------------------
1				|Select the spectrum visualization.  
2				|Select the wave visualization.  
3				|Select the fountain visualization.  
c				|Toggle color.  
p				|Toggle falling peaks.  
k				|Toggle keep state.  
l				|Toggle direction.  
b				|Toggle bounce mode.  
n or [Right]    |Cycle visualizations in ascending order.  
N or [Left]     |Cycle visualizations in descending order.  


Enjoy!
