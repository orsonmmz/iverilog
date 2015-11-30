/*
 * Copyright (c) 2015 CERN
 * @author Maciej Suminski
 *
 * This source code is free software; you can redistribute it
 * and/or modify it in source code form under the terms of the GNU
 * General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "vpi_user.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>
#include "../mhdlsim/msg.h"

void *zmq_ctx;
void *zmq_sender, *zmq_recver;

static PLI_INT32 next_sim_time(p_cb_data cb_data_p)
{
    struct t_cb_data cb;
    (void) cb_data_p;

    // TODO the behaviour below is *WRONG* but is enough for demonstration
    // purposes now we synchronize values with the launcher before a scheduled
    // event is executed which might be fine for synchronous circuits without
    // delays, but in other cases is likely to produce erroneous results

    vpi_printf("next_sim_time\n");

    /*cb.reason   = cbNextSimTime;*/
    /*cb.cb_rtn   = next_sim_time;*/
    /*vpi_register_cb(&cb);*/

    // check if there are any signal updates
    mhdlsim_msg_t msg;
    if(zmq_recv(zmq_recver, &msg, MS_MSG_SIZE, ZMQ_DONTWAIT) > 0)
        vpi_printf("got msg %s %d\n", msg.signal, msg.value);

    return 0;
}

static PLI_INT32 value_change(p_cb_data cb_data_p)
{
    PLI_UINT64 time = ((PLI_UINT64) cb_data_p->time->high << 32) | cb_data_p->time->low;

    // TODO here we should obtain the next event timestamp and report it to
    // the launcher, so it knows which simulator instance should run now
    // (i.e. the one with the shortest time to to the next event)

    // TODO tf_getnextlongtime should return the next event timestamp but is
    // currently not implemented in Icarus

    vpi_printf("value change %lu:\t %s = %d\n",
        time, cb_data_p->user_data, cb_data_p->value->value.scalar);

    // report the value change to the launcher
    mhdlsim_msg_t msg;
    memset(msg.signal, 0, MS_SIGNAL_LEN);
    strncpy(msg.signal, cb_data_p->user_data, MS_SIGNAL_LEN);
    msg.value = cb_data_p->value->value.scalar;
    zmq_send(zmq_sender, &msg, MS_MSG_SIZE, 0);

    return 0;
}

static PLI_INT32 sim_start(p_cb_data cb_data_p)
{
    struct t_cb_data cb;
    s_vpi_time time;
    s_vpi_value value;
    vpiHandle arg_itr, mod_h, net_itr, net_h, cb_h;
    PLI_BYTE8 *net_name_temp, *net_name_keep;
    (void) cb_data_p;

    // TODO initialize zeromq

    // setup value change callback options
    time.type   = vpiSimTime;
    value.format = vpiScalarVal;

    cb.reason   = cbValueChange;
    cb.cb_rtn   = value_change;
    cb.time     = &time;
    cb.value    = &value;

    // register all nets to monitor changes

    // iterate through the top modules
    // TODO traverse the module tree down as well
    arg_itr = vpi_iterate(vpiModule, NULL);

    while((mod_h = vpi_scan(arg_itr)))
    {
        // add value change callback for each net in module named in tfarg
        /*vpi_printf("\nAdding monitors to all nets in module %s:\n\n",*/
        vpi_get_str(vpiDefName, mod_h);
        // TODO types other than net
        net_itr = vpi_iterate(vpiNet, mod_h);

        while ((net_h = vpi_scan(net_itr)) != NULL) {
            net_name_temp = vpi_get_str(vpiFullName, net_h);
            net_name_keep = malloc(strlen((char *)net_name_temp)+1);
            strcpy((char *)net_name_keep, (char *)net_name_temp);
            cb.obj = net_h;
            cb.user_data = net_name_keep;
            cb_h = vpi_register_cb(&cb) ;
            vpi_free_object(cb_h); /* don’t need callback handle */
        }
    }

    // set up IPC
    zmq_ctx = zmq_ctx_new();
    zmq_sender = zmq_socket(zmq_ctx, ZMQ_PUSH);
    zmq_recver = zmq_socket(zmq_ctx, ZMQ_PULL);
    zmq_bind(zmq_sender, "tcp://*:5555");
    zmq_connect(zmq_recver, "tcp://localhost:5556");

    return 0;
}

static PLI_INT32 sim_end(p_cb_data cb_data_p)
{
    (void) cb_data_p;

    zmq_close(zmq_sender);
    zmq_close(zmq_recver);
    zmq_ctx_destroy(zmq_ctx);

    return 0;
}

void vpi_link_register(void)
{
    struct t_cb_data cb;

    cb.reason   = cbStartOfSimulation;
    cb.cb_rtn   = sim_start;
    vpi_register_cb(&cb);

    cb.reason   = cbEndOfSimulation;
    cb.cb_rtn   = sim_end;
    vpi_register_cb(&cb);

    cb.reason   = cbNextSimTime;
    cb.cb_rtn   = next_sim_time;
    vpi_register_cb(&cb);
}

void (*vlog_startup_routines[])(void) = {
      vpi_link_register,
      0
};
