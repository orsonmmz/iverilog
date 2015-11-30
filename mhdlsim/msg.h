#ifndef MHDLSIM_MSG_H
#define MHDLSIM_MSG_H

#define MS_MSG_SIZE    (sizeof(mhdlsim_msg_t))
#define MS_SIGNAL_LEN  16

typedef struct mhdlsim_msg_s {
    char signal[MS_SIGNAL_LEN];
    int value;
} mhdlsim_msg_t;


#endif /* MHDLSIM_MSG_H */
