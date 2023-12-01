#include <pthread.h>
#include <stdio.h> //for printing
#include <stdlib.h> 
#include <time.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h> 
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/types.h> 
#include <netinet/in.h>
#include <netdb.h> 
#include <stdlib.h>
#include <math.h>
#include <rc/button.h>
#include <rc/time.h>
#include <rc/gpio.h>
#include <rc/adc.h>
#include <time.h>
#include<unistd.h>



int period_interval = 1;
int use_farenheight = 1;
int should_stop = 0; 
int log_fd = -1; 
int button_fd;
int exit_flag = 0;


void shutdown_program() {

    rc_gpio_cleanup(1, 18);
    rc_adc_cleanup();
    exit(0);

}
float get_temperatureC() {
    int16_t adc_read= rc_adc_read_raw(0);

    int R0 = 100000;
    float R = 4095.0/adc_read-1.0;

    int B = 4275;   
    R = R0 * R;
    float temperature=1.0/(log(R/R0)/B+1/298.15)-273.15;
    return temperature;
}


float get_temperatureF() {
    float celcius = get_temperatureC();
    return ((celcius * (9.0/5.0)) + 32);
}
void initalize_hardware() {

    button_fd =  rc_gpio_init_event(1, 18, 0, GPIOEVENT_REQUEST_RISING_EDGE);
    if(button_fd  == -1) {
        fprintf(stderr, "Failed init event \n");
        exit(2);
    }

    if(rc_adc_init() == -1){
        fprintf(stderr,"ERROR: failed to run rc_init_adc()\n");
        exit(2);
    }

    int output_init = rc_gpio_init (1, 18, GPIOHANDLE_REQUEST_INPUT);
    if(output_init != 0) {
        fprintf(stderr, "ERROR: failed to initialized GPIOHANDLE \n");
        exit(2);
    }
}

void* thread_temperature_action() {

    while(1) {
        float temperature = get_temperatureF();
        if(use_farenheight == 0) temperature = get_temperatureC();
        time_t rawtime;
        struct tm *info;
        time( &rawtime );
        info = localtime( &rawtime );
        char buffer[50];
        sprintf(buffer, "%d:%d:%d %0.1f\n", info->tm_hour, info->tm_min, info->tm_sec, temperature);
        if(should_stop ==0) fprintf(stdout, buffer);
        if(log_fd != -1 && should_stop==0) {
            write(log_fd, buffer, strlen(buffer));
        }
        sleep(period_interval);
        if(exit_flag == 1) {
            pthread_exit(0);
        }


    }
    

}

void process_command(char* buffer, int length) {
    char off[] = "OFF";
    char log[] = "LOG";
    char start[] = "START";
    char celcius[] = "SCALE=C";
    char faren[] = "SCALE=F";
    char period[] = "PERIOD=";
    char stop[] = "STOP";

    if(length <= 2) return;

    if((size_t)(length) > strlen(period) && strncmp(buffer,period,strlen(period)) == 0) {
        period_interval = atoi(buffer+strlen(period));
        if(log_fd != -1) {
            write(log_fd, buffer, length);
            write(log_fd, "\n", 1);
        }
    }

    if(length >= 3 && strncmp(buffer, log, 3) == 0) {
        if(log_fd != -1) {
            write(log_fd, buffer, length);
            write(log_fd, "\n", 1);
        }
    }

    if(length == 4 && strncmp(buffer, stop, 4) == 0) {
        if(log_fd != -1) {
            write(log_fd, buffer, length);
            write(log_fd, "\n", 1);
        }
        should_stop = 1;
    }

    if(length == 7 && strncmp(buffer, celcius, 7) == 0) {
        if(log_fd != -1) {
            write(log_fd, buffer, length);
            write(log_fd, "\n", 1);
        }
        use_farenheight = 0;
    }

    if(length == 7 && strncmp(buffer, faren, 7) == 0) {
        if(log_fd != -1) {
            write(log_fd, buffer, length);
            write(log_fd, "\n", 1);
        }
        use_farenheight = 1;
    }

    if(length == 5 && strncmp(buffer, start, 5) == 0) {
        if(log_fd != -1) {
            write(log_fd, buffer, length);
            write(log_fd, "\n", 1);
        }
        should_stop = 0;
    }

    
    if(length==3 && strncmp(buffer, off, 3) == 0) {
        write(log_fd, buffer, length);
        write(log_fd, "\n", 1);

        time_t rawtime;
        struct tm *info;
        time( &rawtime );
        info = localtime( &rawtime );
        char shutdown_buffer[50];
        sprintf(shutdown_buffer, "%d:%d:%d SHUTDOWN\n", info->tm_hour, info->tm_min, info->tm_sec);
        fprintf(stdout, shutdown_buffer);
        if(log_fd != -1) {
            write(log_fd, shutdown_buffer, strlen(shutdown_buffer));
        }
        exit_flag = 1; 
    }

    
    return;
}

int main(int argc, char *argv[]) {

    srand(time(0));


    int curr_option;
    const struct option options[] = {
        { "scale",  required_argument, NULL,  's' },
        { "period", required_argument, NULL,  'p' },
     { "log", required_argument, NULL, 'l'},
        { 0, 0, 0, 0}
    };


    char* log_name = NULL;
    while((curr_option = getopt_long(argc, argv, "c:p:s:t:l:o", options, NULL)) != -1)  {
        switch(curr_option) {
            case 's':
                if(*optarg == 'F') {
                    use_farenheight = 1;
                } else if (*optarg == 'C') {
                    use_farenheight = 0;
                } else {
                    fprintf(stderr, "You can only specify f or c for scale ");
                    exit(1);
                }
                break;
            case 'p':
                period_interval = atoi(optarg);
                break;
            case 'l':
                log_name = optarg;
                break;
            default:
                fprintf(stderr, "Use the options --iterations --threads");
                exit(1);
                break;
        }
    }
    if(log_name != NULL) {
        log_fd = open(log_name, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
        // log_file = fopen(log_name, "w");
        if(log_fd == -1) {
            fprintf(stderr, "Opening the log file failed %s \n", strerror(errno));
            exit(1);
        }
    }
    

    initalize_hardware();

    pthread_t temp_thread;
    int rc = pthread_create(&temp_thread, NULL, thread_temperature_action, NULL);
    if(rc != 0) {
        fprintf(stderr, "Failed to initialize the pthread \n");
        exit(1);
    }

    int nfds = 2;
    struct pollfd poll_fds[nfds];

    poll_fds[1].fd = button_fd;
    poll_fds[1].events = POLLIN;
    poll_fds[0].fd = 0;
    poll_fds[0].events = POLLIN;

    char incomplete_buffer[100];
    int pointer_in_buffer = 0;
    while(1) {
        int ret = poll(poll_fds, nfds, -1);
        if (ret <= 0) {
            printf("Polling failed\r\n");
            exit(1); 
        }
        for (int input_fd = 0; input_fd < nfds; input_fd++) {
            if (poll_fds[input_fd].revents & POLLIN) {
                char read_buffer[1000]; 
                int how_much_read = read(poll_fds[input_fd].fd, read_buffer, 1000);
                if (input_fd == 0) {
                    // write(1, read_buffer, how_much_read);
                    int pointer_in_read = 0;
                    while(pointer_in_read < how_much_read) {
                        if(read_buffer[pointer_in_read] == '\n') {
                            incomplete_buffer[pointer_in_buffer] = '\0';
                            process_command(incomplete_buffer, pointer_in_buffer);
                            if(exit_flag == 1) {
                                shutdown_program();
                            }
                            pointer_in_buffer = 0;
                            pointer_in_read ++;
                        } else {
                            incomplete_buffer[pointer_in_buffer] = read_buffer[pointer_in_read];
                            pointer_in_buffer ++;
                            pointer_in_read ++;
                        }
                    }
                }else  {
                    time_t rawtime;
                    struct tm *info;
                    time( &rawtime );
                    info = localtime( &rawtime );
                    char shutdown_buffer[50];
                    sprintf(shutdown_buffer, "%d:%d:%d SHUTDOWN\n", info->tm_hour, info->tm_min, info->tm_sec);
                    fprintf(stdout, shutdown_buffer);
                    if(log_fd != -1) {
                        write(log_fd, shutdown_buffer, strlen(shutdown_buffer));
                    }
                    exit_flag = 1;
                    shutdown_program();
                }
            }

            if (poll_fds[input_fd].revents & POLLERR || poll_fds[input_fd].revents & POLLHUP) {

                printf("Polling failed");
                exit(1);
            }
        }
    }
}
