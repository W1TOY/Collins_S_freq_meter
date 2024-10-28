/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "esp_sleep.h"



/* GPIO avaialbility
4: OK
13-33 : OK
34-39 : Input only
*/

// GPIO to generate the pulse and trigger ISR to read counts
#define GPIO_LEDC_MEASURE_PULSE_OUTPUT    13
#define GPIO_MEASURE_IO_PIN_SEL  (1ULL<<GPIO_LEDC_MEASURE_PULSE_OUTPUT)
#define ESP_INTR_FLAG_DEFAULT 0
#define LEDC_MEASURE_FREQUENCY          (5) // Frequency in Hertz. Set frequency at 4 kHz

// GPIO to generate internal reference frequency for testing or ..
#define GPIO_LEDC_REFERENCE_PULSE_OUTPUT 14
#define LEDC_REFERENCE_FREQUENCY          (20000000) // Frequency in Hertz. Set frequency at 4 kHz

// GPIO to enable Pulse Count
#define GPIO_PCNT_ENABLE_INPUT 12
#define GPIO_PCNT_ENABLE_PIN_MASK  (1ULL<<GPIO_PCNT_ENABLE_INPUT)

// GPIO to measure frequencies
#define GPIO_INPUT_FREQ_REF 27      // to measure 20M reference generated internally, shoudl be exact 20M as same clock is used.

// General PCNT values
#define FCNT_LIMIT  32000
#define NUM_FREQ_CNT_EDGES                         (1)  // count only one edge (rising)



static QueueHandle_t gpio_evt_queue = NULL;
static int cnt_measure_isr = 0;
int pulse_count = 0;

// variables for counting cycles
#define NUM_FCNT_CHANNELS 4
#define MAX_FCNT_CHANNELS 6

pcnt_unit_handle_t pcnt_units[NUM_FCNT_CHANNELS];
pcnt_channel_handle_t pcnt_chans[NUM_FCNT_CHANNELS];
int32_t overflow_cnts[MAX_FCNT_CHANNELS] = {0};
int32_t total_cnts[MAX_FCNT_CHANNELS] = {0};
int32_t measured_freqs[MAX_FCNT_CHANNELS] = {0};
int gpio_num_input_freq[6] = {27, 26, 27, 33, 32, 35};
int gpio_num_enable_count[6] = {12, 12, 12, 12, 12, 12};
char * chan_name[6] = {"Internal Reference", "1", "2", "3", "4", "5"};

static bool example_pcnt_on_reach_0(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) { overflow_cnts[0]++; return true; }
static bool example_pcnt_on_reach_1(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) { overflow_cnts[1]++; return true; }
static bool example_pcnt_on_reach_2(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) { overflow_cnts[2]++; return true; }
static bool example_pcnt_on_reach_3(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) { overflow_cnts[3]++; return true; }
static bool example_pcnt_on_reach_4(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) { overflow_cnts[4]++; return true; }
static bool example_pcnt_on_reach_5(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) { overflow_cnts[5]++; return true; }


static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    cnt_measure_isr++;
    uint32_t gpio_num = (uint32_t) arg;
    for (int i=0; i<NUM_FCNT_CHANNELS; i++)
    {
//        if (i==1) break;
        pulse_count = 0;
        ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_units[i], &pulse_count));    
        ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_units[i]));
        total_cnts[i] = overflow_cnts[i] * FCNT_LIMIT + pulse_count;
        overflow_cnts[i] = 0;
    }
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            
            for (int i=0; i<NUM_FCNT_CHANNELS; i++)
            {
//                if (i==1) break;
                measured_freqs[i] = total_cnts[i] * LEDC_MEASURE_FREQUENCY * 2 / NUM_FREQ_CNT_EDGES;
                printf("freq[%d] = %ld, ", i, measured_freqs[i]);
            }

            printf(", cnt=%d\n", cnt_measure_isr );
        }
    }
}



static void example_ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer_0 = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = LEDC_MEASURE_FREQUENCY,  // Set output frequency at 4 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_0));
    
    ledc_timer_config_t ledc_timer_1 = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_2_BIT,
        .timer_num        = LEDC_TIMER_1,
        .freq_hz          = LEDC_REFERENCE_FREQUENCY,  // Set output frequency at 4 kHz
        .clk_cfg          = SOC_MOD_CLK_APB
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_1));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel_measure = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_LEDC_MEASURE_PULSE_OUTPUT,
        .duty           = 4096, // Set duty to 0%
        .hpoint         = 0
    };
    
    ledc_channel_config_t ledc_channel_reference = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_1,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_LEDC_REFERENCE_PULSE_OUTPUT,
        .duty           = 1, // Set duty to 0%
        .hpoint         = 0
    };


    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_measure));
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_reference));
}

void init_freq_pcnt()
{

    pcnt_unit_config_t unit_configs[NUM_FCNT_CHANNELS];
    pcnt_chan_config_t chan_configS[NUM_FCNT_CHANNELS];
    int watch_points[] = {-FCNT_LIMIT, FCNT_LIMIT};

    pcnt_event_callbacks_t cbs[MAX_FCNT_CHANNELS];
    cbs[0].on_reach = example_pcnt_on_reach_0;
    cbs[1].on_reach = example_pcnt_on_reach_1;
    cbs[2].on_reach = example_pcnt_on_reach_2;
    cbs[3].on_reach = example_pcnt_on_reach_3;
    cbs[4].on_reach = example_pcnt_on_reach_4;
    cbs[5].on_reach = example_pcnt_on_reach_5;

    QueueHandle_t queue[NUM_FCNT_CHANNELS];


    for (int i=0; i<NUM_FCNT_CHANNELS; i++) {


        unit_configs[i].high_limit = FCNT_LIMIT;
        unit_configs[i].low_limit = -FCNT_LIMIT;
        unit_configs[i].intr_priority = 0;

        ESP_ERROR_CHECK(pcnt_new_unit(&unit_configs[i], &pcnt_units[i]));
  //      if (i==1) break;


        chan_configS[i].level_gpio_num = gpio_num_enable_count[i];
        chan_configS[i].edge_gpio_num = gpio_num_input_freq[i];
        ESP_ERROR_CHECK(pcnt_new_channel(pcnt_units[i], &chan_configS[i], &pcnt_chans[i]));

        if (NUM_FREQ_CNT_EDGES == 1) {
            ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chans[i], PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));
        }
        else {
           ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chans[i], PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
        }
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chans[i], PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_HOLD));

        for (size_t n = 0; n < sizeof(watch_points) / sizeof(watch_points[0]); n++) {
            ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_units[i], watch_points[n]));
        }
        queue[i] = xQueueCreate(10, sizeof(int));
        ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_units[i], &cbs[i], queue[i]));
        ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_units[i]));
        ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_units[i]));
        ESP_ERROR_CHECK(pcnt_unit_start(pcnt_units[i]));

    };


}


void app_main(void)
{

    example_ledc_init();
    init_freq_pcnt();
/*
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_MEASURE_IO_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
*/
    //change gpio interrupt type for one pin
    gpio_set_intr_type(GPIO_LEDC_MEASURE_PULSE_OUTPUT, GPIO_INTR_NEGEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_LEDC_MEASURE_PULSE_OUTPUT, gpio_isr_handler, (void*) GPIO_LEDC_MEASURE_PULSE_OUTPUT);

    printf("Minimum free heap size: %"PRIu32" bytes\n", esp_get_minimum_free_heap_size());
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
