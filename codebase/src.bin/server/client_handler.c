
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include "control_program.h"
#include "global_server_variables.h"
#include "utils.h"
#include "coordination_handler.h"
#include "dio_handler.h"
#include "reciever_handler.h"
#include "timing_handler.h"
#include "dds_handler.h"
#include "settings_handler.h"
#include "priority.h"
#include "rtypes.h"
#include "iniparser.h"

extern int verbose;
extern int num_radars, num_channels;
extern void* **radar_channels;
extern pthread_mutex_t controlprogram_list_lock,exit_lock;
extern struct Thread_List_Item *controlprogram_threads;
extern struct TRTimes bad_transmit_times;
extern struct SiteSettings site_settings;
extern dictionary *Site_INI;

int unregister_radar_channel(struct ControlProgram *control_program)
{
  int i,j,status;
  status=0;
  if (control_program!=NULL) {
    for(i=0;i<num_radars;i++) {
      for(j=0;j<num_channels;j++) {
        if (radar_channels[i][j]==(void *)control_program) {
          if(verbose>0) fprintf(stderr,"Unregistering: %d %d :: cp: %p\n",i,j,control_program);
          status++;
          radar_channels[i][j]=NULL;
          control_program->parameters->radar=0;
          control_program->parameters->channel=0;
        }
      }
    }
  }
  return status;
}

struct ControlProgram* find_registered_controlprogram_by_radar_channel(int radar,int channel)
{
  int i,j,r,c;
  struct ControlProgram *control_program;
  control_program=NULL;
  for(i=0;i<num_radars;i++) {
    for(j=0;j<num_channels;j++) {
      r=i+1;
      c=j+1;
      if (radar==r && channel==c) {
        control_program=radar_channels[i][j]; 
        break;
      }
    }
  }
  return  control_program;
}

struct ControlPRM controlprogram_fill_parameters(struct ControlProgram *control_program)
{
  struct ControlPRM control_parameters;      
  struct ControlProgram *cp,*best[num_radars];      
  int priority=99; //Lowest priority wins-- its like golf
  int r,c;
  if (TX_BEAM_PRIORITY |RX_BEAM_PRIORITY |TX_FREQ_PRIORITY|RX_FREQ_PRIORITY|TIME_SEQ_PRIORITY) {  
    for (r=1;r<=num_radars;r++) {
      priority=99;
      best[r-1]=NULL; 
      for (c=1;c<=num_channels;c++) {
        cp=find_registered_controlprogram_by_radar_channel(r,c);
        if (cp!=NULL) {
          if (cp->active!=0) {
            if (cp->parameters!=NULL) {
              if (cp->parameters->priority<priority) {
                best[r-1]=cp;
                priority=cp->parameters->priority;
              }
            }
          }
        }
      }
    }
  }
  if (control_program!=NULL) {
    if (control_program->parameters!=NULL) {
       //strcpy(control_parameters.name,control_program->parameters->name);
       //strcpy(control_parameters.description,control_program->parameters->description);
       control_parameters.radar=control_program->radarinfo->radar;
       control_parameters.channel=control_program->radarinfo->channel;
       r=control_parameters.radar-1;
       control_parameters.current_pulseseq_index=control_program->parameters->current_pulseseq_index;
       control_parameters.priority=control_program->parameters->priority;
       if (TX_BEAM_PRIORITY) {
         control_parameters.tbeam=best[r]->parameters->tbeam;
         control_parameters.tbeamcode=best[r]->parameters->tbeamcode;
         control_parameters.tbeamwidth=best[r]->parameters->tbeamwidth;
         control_parameters.tbeamazm=best[r]->parameters->tbeamazm;
       } else {
         control_parameters.tbeam=control_program->parameters->tbeam;
         control_parameters.tbeamcode=control_program->parameters->tbeamcode;
         control_parameters.tbeamwidth=control_program->parameters->tbeamwidth;
         control_parameters.tbeamazm=control_program->parameters->tbeamazm;
       }
       if (TX_FREQ_PRIORITY) {
         control_parameters.tfreq=best[r]->parameters->tfreq;
         control_parameters.trise=best[r]->parameters->trise;
       } else {
         control_parameters.tfreq=control_program->parameters->tfreq;
         control_parameters.trise=control_program->parameters->trise;
       }
       if (RX_BEAM_PRIORITY) {
         control_parameters.rbeam=best[r]->parameters->rbeam;
         control_parameters.rbeamcode=best[r]->parameters->rbeamcode;
         control_parameters.rbeamwidth=best[r]->parameters->rbeamwidth;
         control_parameters.rbeamazm=best[r]->parameters->rbeamazm;
       } else {
         control_parameters.rbeam=control_program->parameters->rbeam;
         control_parameters.rbeamcode=control_program->parameters->rbeamcode;
         control_parameters.rbeamwidth=control_program->parameters->rbeamwidth;
         control_parameters.rbeamazm=control_program->parameters->rbeamazm;
       }
       if (RX_FREQ_PRIORITY) {
         control_parameters.rfreq=best[r]->parameters->rfreq;
         control_parameters.number_of_samples=best[r]->parameters->number_of_samples;
       } else {
         control_parameters.rfreq=control_program->parameters->rfreq;
         control_parameters.number_of_samples=control_program->parameters->number_of_samples;
       }
       control_parameters.buffer_index=control_program->parameters->buffer_index;
       control_parameters.baseband_samplerate=control_program->parameters->baseband_samplerate;
       control_parameters.filter_bandwidth=control_program->parameters->filter_bandwidth;
       control_parameters.match_filter=control_program->parameters->match_filter;
       control_parameters.status=control_program->parameters->status;
    } else {
    }
  } else {
  }
  return control_parameters;
}

struct ControlPRM* controlprogram_link_parameters(struct ControlPRM *control_parameters)
{
  return control_parameters;

}
struct ControlPRM* controlprogram_verify_parameters(struct ControlPRM *control_parameters)
{
  return control_parameters;
}

int register_radar_channel(struct ControlProgram *control_program,int radar,int channel)
{
  int i,j,r,c,status;
  if (control_program!=NULL) unregister_radar_channel(control_program);
  status=-1;
  for(i=0;i<num_radars;i++) {
    for(j=0;j<num_channels;j++) {
      r=i+1;
      c=j+1;
      if (radar_channels[i][j]==NULL) {
        if (radar<=0) radar=r;
        if (channel<=0) channel=c;
        if (radar==r && channel==c) {
          if (verbose > 0 ) fprintf(stderr,"Registering: %d %d :: radar: %d channel: %d cp: %p\n",i,j,radar,channel,control_program);
          status=1;
          control_program->parameters->radar=radar; 
          control_program->parameters->channel=channel; 
          control_program->radarinfo->radar=radar; 
          control_program->radarinfo->channel=channel; 
          radar_channels[i][j]=control_program;
          break;
        }
      }
    }
  }
  return status;
}

struct ControlProgram *control_init() {
       int i;
       struct ControlProgram *control_program;

       control_program=malloc(sizeof(struct ControlProgram));
       control_program->active=1;
       control_program->clrfreqsearch.start=0;
       control_program->clrfreqsearch.end=0;
       control_program->parameters=malloc(sizeof(struct ControlPRM));
       control_program->state=malloc(sizeof(struct ControlState));
       control_program->radarinfo=malloc(sizeof(struct RadarPRM));
       control_program->data=malloc(sizeof(struct DataPRM));
       control_program->main=NULL;
       control_program->back=NULL;
       control_program->main_address=(uint64)NULL;
       control_program->back_address=(uint64)NULL;
       strcpy(control_program->parameters->name,"Generic Control Program Name - 80");
       strcpy(control_program->parameters->description,"Generic  Control Program  Description - 120");
       control_program->parameters->radar=-1;
       control_program->parameters->channel=-1;
       control_program->parameters->current_pulseseq_index=-1;
       control_program->parameters->priority=50;
       control_program->parameters->tbeam=-1;
       control_program->parameters->tbeamcode=-1;
       control_program->parameters->tbeamwidth=-1;
       control_program->parameters->tbeamazm=-1;
       control_program->parameters->tfreq=-1;
       control_program->parameters->trise=10;
       control_program->parameters->rbeam=-1;
       control_program->parameters->rbeamcode=-1;
       control_program->parameters->rbeamwidth=-1;
       control_program->parameters->rbeamazm=-1;
       control_program->parameters->rfreq=-1;
       control_program->parameters->number_of_samples=-1;
       control_program->parameters->buffer_index=-1;
       control_program->parameters->baseband_samplerate=-1;
       control_program->parameters->filter_bandwidth=-1;
       control_program->parameters->match_filter=-1;
       control_program->parameters->status=-1;


//       control_program->parameters->phased=-1;
//       control_program->parameters->filter=-1;
//       control_program->parameters->gain=-1;
//       control_program->parameters->seq_no=-1;
//       control_program->parameters->seq_id=-1;
//       control_program->parameters->fstatus=-1;
//       control_program->parameters->center_freq=-1;

       control_program->state->cancelled=0;
       control_program->state->ready=0;
       control_program->state->linked=0;
       control_program->state->processing=0;
       control_program->state->best_assigned_freq=0;
       control_program->state->current_assigned_freq=0;
       control_program->state->freq_change_needed=0;
       control_program->state->thread=NULL;
       control_program->state->fft_array=NULL;
       control_program->radarinfo->site=-1;
       control_program->radarinfo->radar=-1;
       control_program->radarinfo->channel=-1;

       for (i=0;i<MAX_SEQS;i++) {
         control_program->state->pulseseqs[i]=NULL;
       }
       return control_program;
}

void controlprogram_exit(struct ControlProgram *control_program)
{
   pthread_t tid;
   pthread_t thread;
   int i,rc;
   pthread_t threads[10];
   if (verbose>0) fprintf(stderr,"Client Exit: Start\n");
   if(control_program!=NULL) {
     if (verbose>0) fprintf(stderr,"Client Exit: control program %p\n",control_program); 
     control_program->state->cancelled=1;
     pthread_mutex_lock(&exit_lock);
     tid = pthread_self();
     control_program->active=0;
     rc = pthread_create(&thread, NULL, (void *)&coordination_handler,(void *) control_program);
     pthread_join(thread,NULL);
     i=0;
     rc = pthread_create(&threads[i], NULL,(void *) &timing_wait, NULL);
     pthread_join(threads[0],NULL);
     i=0;
     rc = pthread_create(&threads[i], NULL, (void *) &dds_end_controlprogram, (void *) control_program);
       pthread_join(threads[i],NULL);
     i++;
     rc = pthread_create(&threads[i], NULL, (void *) &timing_end_controlprogram, (void *) control_program);
       pthread_join(threads[i],NULL);
     i++;
     rc = pthread_create(&threads[i], NULL, (void *) &receiver_end_controlprogram, (void *) control_program);
       pthread_join(threads[i],NULL);
     for (;i>=0;i--) {
       pthread_join(threads[i],NULL);
     }
     close(control_program->state->socket);
     unregister_radar_channel(control_program);
     for (i=0;i<MAX_SEQS;i++) {
       if(control_program->state->pulseseqs[i]!=NULL) TSGFree(control_program->state->pulseseqs[i]);
     }
     if(control_program->state!=NULL) {
       if(control_program->state->fft_array!=NULL) {
         free(control_program->state->fft_array);
         control_program->state->fft_array=NULL;
       }
       free(control_program->state);
       control_program->state=NULL;
     }
     if(control_program->parameters!=NULL) {
       free(control_program->parameters);
       control_program->parameters=NULL;
     }
     if(control_program->main!=NULL) munmap(control_program->main,control_program->data->samples);
     if(control_program->back!=NULL) munmap(control_program->back,control_program->data->samples);
     control_program->main=NULL;
     control_program->back=NULL;
     control_program->main_address=(uint64)NULL;
     control_program->back_address=(uint64)NULL;
     if(control_program->data!=NULL) {
       free(control_program->data);
       control_program->data=NULL;
     }
     control_program->active=0;
     if (verbose>0) fprintf(stderr,"Client Exit: Done with control program %p\n",control_program); 
   }
   pthread_mutex_unlock(&exit_lock);
   if (verbose>0) fprintf(stderr,"Client Exit: Done\n");
}

void *control_handler(struct ControlProgram *control_program)
{
   int tid,i,r=-1,c=-1,status,rc;
   fd_set rfds;
   int retval,socket,socket_err;
   unsigned int length=sizeof(int);
   int32 current_freq,radar=0,channel=0;
   struct timeval tv,current_time,last_report;
   struct ROSMsg msg; 
   struct ControlPRM control_parameters; 
   struct SiteSettings settings;
//   struct TSGprm *tsgprm;
   struct SeqPRM tprm;
   pthread_t thread,threads[10];
   int32 data_length;
   char entry_type,entry_name[200];
   int return_type,entry_exists;
   char *temp_strp;
   int32 temp_int32;
   dictionary *aux_dict=NULL;
/*
*  Init the Control Program state
*/
   pthread_mutex_lock(&exit_lock);
   pthread_mutex_lock(&controlprogram_list_lock);

   setbuf(stdout, 0);
   setbuf(stderr, 0);
   tid = pthread_self();
/* set the cancellation parameters --
   - Enable thread cancellation 
   - Defer the action of the cancellation 
*/
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
   pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
   pthread_cleanup_push((void *)&controlprogram_exit,(void *)control_program);
     if(control_program!=NULL) {
       socket=control_program->state->socket;
     }
   pthread_mutex_unlock(&controlprogram_list_lock);
   pthread_mutex_unlock(&exit_lock);

   gettimeofday(&last_report,NULL);
   while (1) {
      if(control_program==NULL) {
        break;
      }
      retval=getsockopt(socket, SOL_SOCKET, SO_ERROR, &socket_err, &length);
      if ((retval!=0) || (socket_err!=0)) {
            fprintf(stderr,"Error: socket error: %d : %d %d\n",socket,retval,socket_err);
        break;
      }
      /* Look for messages from external controlprogram process */
      FD_ZERO(&rfds);
      FD_SET(socket, &rfds);
           /* Wait up to five seconds. */
      tv.tv_sec = 5;
      tv.tv_usec = 0;
      retval = select(socket+1, &rfds, NULL, NULL, &tv);
           /* Don’t rely on the value of tv now! */
      if (retval == -1) perror("select()");
      else if (retval) {
        pthread_mutex_lock(&controlprogram_list_lock);
        r=control_program->radarinfo->radar-1;
        c=control_program->radarinfo->channel-1;
        if ((r<0) || (c<0)) control_program->data->status=-1;
 
       /* Read controlprogram msg */
        msg.status=-300;
        msg.type=0;
        recv_data(socket, &msg, sizeof(struct ROSMsg));
        gettimeofday(&current_time,NULL);
        if((current_time.tv_sec-last_report.tv_sec)>5) {
#ifdef __QNX__
          system("date -t > /tmp/server_cmd_time");
#else
          system("date +'%s' > /tmp/server_cmd_time");
#endif
          last_report=current_time;
        }
        if(msg.type!=0) {
          control_program->state->thread->last_seen=current_time;
        }
        pthread_mutex_unlock(&controlprogram_list_lock);
        /* Process controlprogram msg */
        switch(msg.type) {
          case 0:
            fprintf(stderr,"Bad Message Sleeping 1 second\n");
            sleep(1);
            break;
	  case AUX_COMMAND:
            /* AUX_COMMAND: Site hardware specific commands which are not critical for operation, but
             *  controlprograms may optionally access to if they are site aware.
             */
             /* Inform the ROS that this driver does not handle this command by sending 
              * msg back with msg.status=0.
              */
            msg.status=1;
            if(msg.status==1) {
                send_data(socket, &msg, sizeof(struct ROSMsg));
                if(aux_dict!=NULL) iniparser_freedict(aux_dict);
                recv_aux_dict(socket,&aux_dict,0);
                iniparser_dump_ini(aux_dict,stdout);
                /* process aux command dictionary here */
                process_aux_commands(&aux_dict,"DIO");
                send_aux_dict(socket,aux_dict,1);
            }
            send_data(socket, &msg, sizeof(struct DriverMsg));
            break;

          case PING:
            msg.status=1;
            send_data(socket, &msg, sizeof(struct ROSMsg));
            break;
          case SET_INACTIVE:
            if ( (r < 0) || (c < 0)) {
              msg.status=-1;
              send_data(socket, &msg, sizeof(struct ROSMsg));
            } else {
              pthread_mutex_lock(&controlprogram_list_lock);
              if(control_program->active!=0) {
                control_program->active=-1;
                control_program->state->ready=0;
                rc = pthread_create(&thread, NULL, (void *)&coordination_handler,(void *) control_program);
                pthread_join(thread,NULL);
              }
              pthread_mutex_unlock(&controlprogram_list_lock);
              msg.status=1;
              send_data(socket, &msg, sizeof(struct ROSMsg));
            }
            break;
          case SET_ACTIVE:
            if ( (r < 0) || (c < 0)) {
              msg.status=-1;
              send_data(socket, &msg, sizeof(struct ROSMsg));
            } else {
              pthread_mutex_lock(&controlprogram_list_lock);
              if(control_program->active!=0) {
                control_program->active=1;
                control_program->state->ready=0;
                rc = pthread_create(&thread, NULL, (void *)&coordination_handler,(void *) control_program);
                pthread_join(thread,NULL);
              }
              pthread_mutex_unlock(&controlprogram_list_lock);
              msg.status=1;
              send_data(socket, &msg, sizeof(struct ROSMsg));
            }
            break;
          case QUERY_INI_SETTING:
            recv_data(socket, &data_length, sizeof(int32));
            recv_data(socket, &entry_name, data_length*sizeof(char));
            recv_data(socket, &entry_type, sizeof(char));
            entry_exists=iniparser_find_entry(Site_INI,entry_name);
            msg.status=entry_exists;
            switch(entry_type) {
              case 'i':
                return_type='i';
                temp_int32=iniparser_getint(Site_INI,entry_name,-1);
                send_data(socket, &return_type, sizeof(char));
                data_length=1;
                send_data(socket, &data_length, sizeof(int32));
                send_data(socket, &temp_int32, data_length*sizeof(int32));
                break;
              case 'b':
                return_type='b';
                temp_int32=iniparser_getboolean(Site_INI,entry_name,-1);
                send_data(socket, &return_type, sizeof(char));
                data_length=1;
                send_data(socket, &data_length, sizeof(int32));
                send_data(socket, &temp_int32, data_length*sizeof(int32));
                break;
              case 's':
                return_type='s';
                temp_strp=iniparser_getstring(Site_INI,entry_name,NULL);
                send_data(socket, &return_type, sizeof(char));
                data_length=strlen(temp_strp)+1;
                send_data(socket, &data_length, sizeof(int32));
                send_data(socket, temp_strp, data_length*sizeof(char));
                break;
              default:
                return_type=' ';
                send_data(socket, &return_type, sizeof(char));
                data_length=0;
                send_data(socket, &data_length, sizeof(int32));
                send_data(socket, temp_strp, data_length*sizeof(char));
            }
            send_data(socket, &msg, sizeof(struct ROSMsg));
            break;
          case GET_SITE_SETTINGS:
            settings=site_settings;
            send_data(socket, &settings, sizeof(struct SiteSettings));
            msg.status=-1;
            send_data(socket, &msg, sizeof(struct ROSMsg));
            break;
          case SET_SITE_IFMODE:
            settings=site_settings;
            recv_data(socket, &settings.ifmode, sizeof(settings.ifmode));
            msg.status=-1;
            send_data(socket, &msg, sizeof(struct ROSMsg));
            break;
          case SET_RADAR_CHAN:
              msg.status=1;
              recv_data(socket, &radar, sizeof(int32)); //requested radar
              recv_data(socket, &channel, sizeof(int32)); //requested channel
              pthread_mutex_lock(&controlprogram_list_lock);
              status=register_radar_channel(control_program,radar,channel);
              if (status) {
              }
              else {
                if (verbose>-1) fprintf(stderr,"Control Program thread %d Bad status %d no radar channel registered\n", tid,status);
              }
              msg.status=status;
              pthread_mutex_unlock(&controlprogram_list_lock);
              send_data(socket, &msg, sizeof(struct ROSMsg));
            break;
          case LINK_RADAR_CHAN:
            msg.status=1;
            recv_data(socket, &r, sizeof(r)); //requested radar
            recv_data(socket, &c, sizeof(c)); //requested channel
            pthread_mutex_lock(&controlprogram_list_lock);
            control_program->state->linked_program=find_registered_controlprogram_by_radar_channel(r,c);
            control_program->state->linked=1;
            if (control_program->state->linked_program!=NULL) {
              status=1;
            }
            else {
              status=0;
            }
            msg.status=status;
            pthread_mutex_unlock(&controlprogram_list_lock);
            send_data(socket, &msg, sizeof(struct ROSMsg));
            break;
          case GET_PARAMETERS:
            if ( (r < 0) || (c < 0)) {
              send_data(socket, &control_parameters, sizeof(struct ControlPRM));
              msg.status=-1;
              send_data(socket, &msg, sizeof(struct ROSMsg));
            } else {
              pthread_mutex_lock(&controlprogram_list_lock);
              msg.status=status;
              control_parameters=controlprogram_fill_parameters(control_program);
              pthread_mutex_unlock(&controlprogram_list_lock);
              send_data(socket, &control_parameters, sizeof(struct ControlPRM));
              send_data(socket, &msg, sizeof(struct ROSMsg));
            }
            break;
          case GET_DATA:
            if ( (r < 0) || (c < 0)) {
              control_program->data->status=-1;
              send_data(socket, control_program->data, sizeof(struct DataPRM));
              msg.status=-1;
              send_data(socket, &msg, sizeof(struct ROSMsg));
            } else {
              msg.status=status;
              rc = pthread_create(&thread, NULL,(void *)&receiver_controlprogram_get_data,(void *) control_program);
              pthread_join(thread,NULL);
//JDS: TODO do some GPS timestamp checking here
              pthread_mutex_lock(&controlprogram_list_lock);
              control_program->data->event_secs=control_program->state->gpssecond;
              control_program->data->event_nsecs=control_program->state->gpsnsecond;
              send_data(socket, control_program->data, sizeof(struct DataPRM));
              if(control_program->data->status>0) {
                send_data(socket, control_program->main, sizeof(uint32)*control_program->data->samples);
                send_data(socket, control_program->back, sizeof(uint32)*control_program->data->samples);
                send_data(socket, &bad_transmit_times.length, sizeof(bad_transmit_times.length));
                send_data(socket, bad_transmit_times.start_usec, sizeof(uint32)*bad_transmit_times.length);
                send_data(socket, bad_transmit_times.duration_usec, sizeof(uint32)*bad_transmit_times.length);
              } else {
                if (verbose > -1 ) fprintf(stderr,"CLIENT:GET_DATA: Bad status %d\n",control_program->data->status);
              } 
              send_data(socket, &msg, sizeof(struct ROSMsg));
              pthread_mutex_unlock(&controlprogram_list_lock);
            }
            break;
          case SET_PARAMETERS:
            if ( (r < 0) || (c < 0)) {
              recv_data(socket, control_program->parameters, sizeof(struct ControlPRM));
              msg.status=-1;
              send_data(socket, &msg, sizeof(struct ROSMsg));
            } else {
              msg.status=1;
              pthread_mutex_lock(&controlprogram_list_lock);
              recv_data(socket, control_program->parameters, sizeof(struct ControlPRM));
              if(control_program->parameters->rfreq<0) control_program->parameters->rfreq=control_program->parameters->tfreq;
              send_data(socket, &msg, sizeof(struct ROSMsg));
              pthread_mutex_unlock(&controlprogram_list_lock);
            }
            break;
          case REGISTER_SEQ:
            msg.status=1;
            recv_data(socket,&tprm, sizeof(struct SeqPRM)); // requested pulseseq
            pthread_mutex_lock(&controlprogram_list_lock);
            control_program->state->pulseseqs[tprm.index]=malloc(sizeof(struct TSGbuf));
            control_program->parameters->current_pulseseq_index=tprm.index;
            control_program->state->pulseseqs[tprm.index]->len=tprm.len;
            control_program->state->pulseseqs[tprm.index]->step=tprm.step;
            control_program->state->pulseseqs[tprm.index]->index=tprm.index;
            control_program->state->pulseseqs[tprm.index]->prm=NULL;
            control_program->state->pulseseqs[tprm.index]->rep=
                malloc(sizeof(unsigned char)*control_program->state->pulseseqs[tprm.index]->len);
            control_program->state->pulseseqs[tprm.index]->code=
                malloc(sizeof(unsigned char)*control_program->state->pulseseqs[tprm.index]->len);
            control_program->state->pulseseqs[tprm.index]->prm=malloc(sizeof(struct TSGprm));
            recv_data(socket,control_program->state->pulseseqs[tprm.index]->rep, 
                sizeof(unsigned char)*control_program->state->pulseseqs[tprm.index]->len); // requested pulseseq
            recv_data(socket,control_program->state->pulseseqs[tprm.index]->code, 
                sizeof(unsigned char)*control_program->state->pulseseqs[tprm.index]->len); // requested pulseseq
            if ( (r < 0) || (c < 0)) {
              msg.status=-1;
            } else {
            //send on to timing socket
              rc = pthread_create(&threads[0], NULL, (void *)&timing_register_seq,(void *) control_program);
            //send on to dds socket
              rc = pthread_create(&threads[1], NULL, (void *)&dds_register_seq,(void *) control_program);
              pthread_join(threads[0],NULL);
              pthread_join(threads[1],NULL);
            }
            pthread_mutex_unlock(&controlprogram_list_lock);
            send_data(socket, &msg, sizeof(struct ROSMsg));
            break;
          case SET_READY_FLAG:
            if ( (r < 0) || (c < 0)) {
              msg.status=-1;
            } else {
              msg.status=0;
              pthread_mutex_lock(&controlprogram_list_lock);
              if (control_program->active!=0) control_program->active=1;
              pthread_mutex_unlock(&controlprogram_list_lock);
              i=0;
              rc = pthread_create(&threads[i], NULL,(void *) &timing_wait, NULL);
              pthread_join(threads[0],NULL);
              pthread_mutex_lock(&controlprogram_list_lock);
              control_program->state->ready=1;
              i=0;
              rc = pthread_create(&threads[i], NULL, (void *) &DIO_ready_controlprogram, control_program);
              i++;
              rc = pthread_create(&threads[i], NULL, (void *) &timing_ready_controlprogram, control_program);
              i++;
              rc = pthread_create(&threads[i], NULL, (void *) &dds_ready_controlprogram, control_program);
              i++;
              rc = pthread_create(&threads[i], NULL, (void *) &receiver_ready_controlprogram, control_program);
              for (;i>=0;i--) {
                pthread_join(threads[i],NULL);
              }
              rc = pthread_create(&thread, NULL, (void *)&coordination_handler,(void *) control_program);
              pthread_join(thread,NULL);
              pthread_mutex_unlock(&controlprogram_list_lock);
            }
            send_data(socket, &msg, sizeof(struct ROSMsg));
            break;

          case REQUEST_CLEAR_FREQ_SEARCH:
            pthread_mutex_lock(&controlprogram_list_lock);
            recv_data(socket,&control_program->clrfreqsearch, sizeof(struct CLRFreqPRM)); // requested search parameters
            if ( (r < 0) || (c < 0)) {
              msg.status=-1;
            } else {
              rc = pthread_create(&threads[0], NULL, (void *) &DIO_pre_clrfreq,control_program);
              pthread_join(threads[0],NULL);
              rc = pthread_create(&threads[0], NULL, (void *) &receiver_clrfreq,control_program);
              pthread_join(threads[0],NULL);
              rc = pthread_create(&threads[0], NULL, (void *) &DIO_post_clrfreq,NULL);
              pthread_join(threads[0],NULL);
              msg.status=control_program->state->freq_change_needed;
            }
            send_data(socket, &msg, sizeof(struct ROSMsg));
            pthread_mutex_unlock(&controlprogram_list_lock);
            break;
          case REQUEST_ASSIGNED_FREQ:
            pthread_mutex_lock(&controlprogram_list_lock);
            if ( (r < 0) || (c < 0)) {
              msg.status=-1;
              control_program->state->current_assigned_freq=0;
              control_program->state->current_assigned_noise=0;
            } else {
              rc = pthread_create(&threads[0], NULL, (void *) &receiver_assign_frequency,(void *)  control_program);
              pthread_join(threads[0],NULL);
              msg.status=control_program->state->best_assigned_freq!=control_program->state->current_assigned_freq;
            }
            current_freq=control_program->state->current_assigned_freq; 
            send_data(socket, &current_freq, sizeof(int32));
            send_data(socket, &control_program->state->current_assigned_noise, sizeof(float));
            send_data(socket, &msg, sizeof(struct ROSMsg));
            pthread_mutex_unlock(&controlprogram_list_lock);
            break;

          case QUIT:
            if (verbose > 0 ) fprintf(stderr,"Client QUIT\n");
            msg.status=0;
            send_data(socket, &msg, sizeof(struct ROSMsg));
            //controlprogram_exit(control_program);
            pthread_exit(NULL);
            break;
          default:
            msg.status=1;
            send_data(socket, &msg, sizeof(struct ROSMsg));
        }
          /* FD_ISSET(0, &rfds) will be true. */
      } else { 
        if (verbose > 1 ) fprintf(stderr,"No data within select timeout\n");
      }
      pthread_testcancel();
   }
   pthread_testcancel();
   pthread_cleanup_pop(0);
   controlprogram_exit(control_program);
   pthread_exit(NULL);
};

void *controlprogram_free(struct ControlProgram *control_program)
{
  pthread_exit(NULL);
}
