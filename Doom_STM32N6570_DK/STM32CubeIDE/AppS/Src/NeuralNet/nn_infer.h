// nn_infer.h
// Pure C neural network inference for Doom Bot
// No external dependencies. Runs on PC and STM32N6.

#ifndef NN_INFER_H
#define NN_INFER_H

// Output structure from inference
typedef struct {
    int fwd_class;    // 0=backward, 1=stop, 2=forward
    int side_class;   // 0=right, 1=none, 2=left
    int turn_class;   // 0=hard_R, 1=slight_R, 2=straight, 3=slight_L, 4=hard_L
    int fire;         // 0=no, 1=yes
    int use;          // 0=no, 1=yes
} nn_result_t;

// Run inference on raw (pre-normalization) feature vector
// Input: 34 floats (player state + rays + monsters), same scale as CSV
// Output: classified actions
void NN_Infer(const float *raw_features, nn_result_t *result);

#endif
