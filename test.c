#include <stdio.h>
#include <stdbool.h>

typedef struct {
    char sensor_type[5];
    int x;
    int y;
    int z;
    unsigned int timestamp;
} SensorData;

typedef struct {
    SensorData data;
    bool is_valid;
} TempData;

void processData(SensorData sensor_data);

#define TARGET_INTERVAL 20000
#define MAX_DATA_POINTS 1000
#define TOLERANCE 1000
#define READY_BUFFER_SIZE 45
#define READY_PACK_BUFFER_SIZE 45
ReadyPack ready_pack[READY_PACK_BUFFER_SIZE];
size_t ready_pack_index = 0;

// 丢点统计
unsigned int acc_data_loss_count = 0;
unsigned int gyro_data_loss_count = 0;



int data_ready = 0;
typedef struct {
    float acc_x, acc_y, acc_z;
    float gyro_x, gyro_y, gyro_z;
} ReadyPack;
ReadyPack ready_pack;

TempData acc_temp = { .is_valid = false };
TempData gyro_temp = { .is_valid = false };
SensorData acc_ready[MAX_DATA_POINTS];
SensorData gyro_ready[MAX_DATA_POINTS];
size_t acc_ready_index = 0;
size_t gyro_ready_index = 0;

// 检查是否推给pusong数据
void check_can_push() {
    while (acc_ready_index > 0 && gyro_ready_index > 0) {
        unsigned int timestamp_diff = abs(acc_ready[0].timestamp - gyro_ready[0].timestamp);
        if (timestamp_diff < TARGET_INTERVAL) {
            if (ready_pack_index == READY_PACK_BUFFER_SIZE) {
                for (size_t i = 0; i < READY_PACK_BUFFER_SIZE - 1; i++) {
                    ready_pack[i] = ready_pack[i + 1];
                }
                ready_pack_index--;
            }
            ready_pack[ready_pack_index].acc_x = acc_ready[0].x;
            ready_pack[ready_pack_index].acc_y = acc_ready[0].y;
            ready_pack[ready_pack_index].acc_z = acc_ready[0].z;
            ready_pack[ready_pack_index].gyro_x = gyro_ready[0].x;
            ready_pack[ready_pack_index].gyro_y = gyro_ready[0].y;
            ready_pack[ready_pack_index].gyro_z = gyro_ready[0].z;
            ready_pack_index++;
            ready_queue_pop(acc_ready, &acc_ready_index);
            ready_queue_pop(gyro_ready, &gyro_ready_index);
        } else {
            if (acc_ready[0].timestamp < gyro_ready[0].timestamp) {
                ready_queue_pop(acc_ready, &acc_ready_index);
            } else {
                ready_queue_pop(gyro_ready, &gyro_ready_index);
            }
        }
    }
}


void ready_queue_pop(SensorData* ready_data, size_t* ready_index) {
    for (size_t i = 0; i < *ready_index - 1; i++) {
        ready_data[i] = ready_data[i + 1];
    }
    (*ready_index)--;
}

void ready_queue_push(SensorData* ready_data, size_t* ready_index) {
    if (*ready_index < READY_BUFFER_SIZE) {
        ready_data[*ready_index] = new_data;
        (*ready_index)++;
    } else {
        ready_queue_pop(ready_data, ready_index);
        ready_data[*ready_index] = new_data;
    }
}

void clear_buffers() {
    acc_temp.is_valid = false;
    acc_temp.is_valid = false;
    acc_ready_index = 0;
    gyro_ready_index = 0;
}


void processData(SensorData sensor_data) {
    TempData* temp_data;
    SensorData* ready_data;
    size_t* ready_index;

    if (strcmp(sensor_data.sensor_type, "ACC") == 0) {
        temp_data = &acc_temp;
        ready_data = acc_ready;
        ready_index = &acc_ready_index;
    } else if (strcmp(sensor_data.sensor_type, "GYRO") == 0) {
        temp_data = &gyro_temp;
        ready_data = gyro_ready;
        ready_index = &gyro_ready_index;
    } else {
        return; // 如果传感器类型不是 ACC 或 GYRO，则直接返回
    }

    if (temp_data->is_valid) {
        unsigned int time_diff = sensor_data.timestamp - temp_data->data.timestamp;
        if (time_diff <= 0) {
            // skip
            return;
        }
        if (time_diff > 80000) {
            // 丢太多reset
            need_reset = 1;
            clear_buffers();
        } else if (time_diff < TARGET_INTERVAL - TOLERANCE) {
            // 当时间差小于目标间隔时，将此数据点存入临时缓存
            temp_data->data = sensor_data;
        } else if (time_diff > TARGET_INTERVAL + TOLERANCE) {
            // 线性插值生成新的数据点
            while (time_diff >= TARGET_INTERVAL + TOLERANCE) {

                if (sensor_type == ACC) {
                    acc_data_loss_count++;
                } else if (sensor_type == GYRO) {
                    gyro_data_loss_count++;
                }

                float ratio = (float)TARGET_INTERVAL / time_diff;
                SensorData new_data = {
                    .sensor_type = sensor_data.sensor_type,
                    .x = temp_data->data.x + ratio * (sensor_data.x - temp_data->data.x),
                    .y = temp_data->data.y + ratio * (sensor_data.y - temp_data->data.y),
                    .z = temp_data->data.z + ratio * (sensor_data.z - temp_data->data.z),
                    .timestamp = temp_data->data.timestamp + TARGET_INTERVAL
                };

                // 将新生成的数据点存入 ready 缓存数组
                ready_queue_push(ready_data, ready_index);

                // 更新临时缓存数据和时间差
                temp_data->data = new_data;
                time_diff -= TARGET_INTERVAL;
            }
            // 检查剩余的时间差是否在允许范围内
            if (time_diff >= TARGET_INTERVAL - TOLERANCE && time_diff <= TARGET_INTERVAL + TOLERANCE) {
                // 当时间差在目标间隔的容差范围内时，将此数据点存入 ready 缓存数组
                ready_queue_push(ready_data, ready_index);
            }
            // 处理完所有插值数据后，将当前数据点存入临时缓存
            temp_data->data = sensor_data;
        } else {
            // 当时间差在目标间隔的容差范围内时，将此数据点存入 ready 缓存数组
            ready_queue_push(ready_data, ready_index);
            // 更新临时缓存数据
            temp_data->data = sensor_data;
        }
    } else {
        // 如果临时缓存为空，且当前数据点的时间戳与目标间隔相差较小，则直接将数据点存入 ready 缓存数组
        int timestamp_mod = sensor_data.timestamp % TARGET_INTERVAL;
        if (timestamp_mod < TARGET_INTERVAL / 2 + TOLERANCE && timestamp_mod > TARGET_INTERVAL / 2 - TOLERANCE) {
            ready_queue_push(ready_data, ready_index);
        }
        // 将当前数据点存入临时缓存，并设置 is_valid 为 true
        temp_data->data = sensor_data;
        temp_data->is_valid = true;
    }
    // 检查是否可以推送
    check_can_push();
}
