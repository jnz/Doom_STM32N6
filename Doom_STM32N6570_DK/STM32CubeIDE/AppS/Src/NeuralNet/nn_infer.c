// nn_infer.c
// Pure C neural network inference. No dependencies.
// Architecture: input -> 128 (ReLU) -> 64 (ReLU) -> 32 (ReLU) -> 5 heads

#include "nn_infer.h"
#include "nn_weights.h"

static void linear_relu(const float *input, int n_in,
                        const float *weight, const float *bias,
                        float *output, int n_out)
{
    int i, j;
    for (i = 0; i < n_out; i++)
    {
        float sum = bias[i];
        for (j = 0; j < n_in; j++)
            sum += weight[i * n_in + j] * input[j];
        output[i] = (sum > 0.0f) ? sum : 0.0f;
    }
}

static void linear(const float *input, int n_in,
                   const float *weight, const float *bias,
                   float *output, int n_out)
{
    int i, j;
    for (i = 0; i < n_out; i++)
    {
        float sum = bias[i];
        for (j = 0; j < n_in; j++)
            sum += weight[i * n_in + j] * input[j];
        output[i] = sum;
    }
}

static int argmax(const float *arr, int n)
{
    int best = 0, i;
    for (i = 1; i < n; i++)
        if (arr[i] > arr[best])
            best = i;
    return best;
}

void NN_Infer(const float *raw_features, nn_result_t *result)
{
    float scaled[NN_INPUT_SIZE];
    float h1[NN_HIDDEN1];
    float h2[NN_HIDDEN2];
    float h3[NN_HIDDEN3];
    float out_buf[5];
    int i;

    // Pre-normalization (must match train_doom_bot.py exactly)
    for (i = 0; i < NN_INPUT_SIZE; i++)
        scaled[i] = raw_features[i];

    // ---------------
    // !!! WARNING !!!
    // ---------------
    // If you change the scaling here, adjust the training script in
    // ./training/train_doom_bot.py as well.

    // Player [0..4]: angle, health, momx, momy, weapon
    // scaled[0] already [0,1)
    scaled[1] /= 200.0f;    // health
    scaled[2] /= 30.0f;     // momx
    scaled[3] /= 30.0f;     // momy
    scaled[4] /= 8.0f;      // weapon

    // Rays [5..20]
    for (i = 5; i < 21; i++)
        scaled[i] /= 2048.0f;

    // Monsters [21..29]: groups of (dx, dy, present)
    for (i = 0; i < 3; i++)
    {
        int base = 21 + i * 3;
        scaled[base]     /= 2048.0f;  // dx
        scaled[base + 1] /= 2048.0f;  // dy
        // scaled[base + 2] is already 0/1 (present)
    }

    // Last action [30..34]: already small integers, normalize
    scaled[30] /= 2.0f;   // fwd_class [0,1,2] -> [0, 1]
    scaled[31] /= 2.0f;   // side_class [0,1,2] -> [0, 1]
    scaled[32] /= 4.0f;   // turn_class [0,1,2,3,4] -> [0, 1]
    // scaled[33], scaled[34] already 0/1 (fire, use)

    // StandardScaler
    for (i = 0; i < NN_INPUT_SIZE; i++)
        scaled[i] = (scaled[i] - scaler_mean[i]) / scaler_scale[i];

    // Forward pass
    linear_relu(scaled, NN_INPUT_SIZE, layer1_weight, layer1_bias, h1, NN_HIDDEN1);
    linear_relu(h1, NN_HIDDEN1, layer2_weight, layer2_bias, h2, NN_HIDDEN2);
    linear_relu(h2, NN_HIDDEN2, layer3_weight, layer3_bias, h3, NN_HIDDEN3);

    // Output heads
    linear(h3, NN_HIDDEN3, head_fwd_weight, head_fwd_bias, out_buf, NN_OUT_FWD);
    result->fwd_class = argmax(out_buf, NN_OUT_FWD);

    linear(h3, NN_HIDDEN3, head_side_weight, head_side_bias, out_buf, NN_OUT_SIDE);
    result->side_class = argmax(out_buf, NN_OUT_SIDE);

    linear(h3, NN_HIDDEN3, head_turn_weight, head_turn_bias, out_buf, NN_OUT_TURN);
    result->turn_class = argmax(out_buf, NN_OUT_TURN);

    linear(h3, NN_HIDDEN3, head_fire_weight, head_fire_bias, out_buf, NN_OUT_FIRE);
    result->fire = argmax(out_buf, NN_OUT_FIRE);

    linear(h3, NN_HIDDEN3, head_use_weight, head_use_bias, out_buf, NN_OUT_USE);
    result->use = argmax(out_buf, NN_OUT_USE);
}
