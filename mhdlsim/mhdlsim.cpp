#include <iostream>
#include <zmq.hpp>

#include "msg.h"

struct sim_instance {

};

int main (void) {
    mhdlsim_msg_t msg;

    // Socket to talk to the clients
    zmq::context_t context(1);
    zmq::socket_t recver(context, ZMQ_PULL);
    zmq::socket_t sender(context, ZMQ_PULL);
    sender.bind("tcp://*:5556");
    recver.connect("tcp://localhost:5555");

    while(recver.connected()) {
        try {
            recver.recv(&msg, MS_MSG_SIZE);
            sender.send(&msg, MS_MSG_SIZE, ZMQ_DONTWAIT);
        } catch(zmq::error_t& err) {
            break;
        }

        printf("signal %s = %d\n", msg.signal, msg.value);
    }

    return 0;
}
