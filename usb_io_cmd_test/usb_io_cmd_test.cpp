// usb_io_cmd_test.cpp

#if defined(WIN32) || defined(_WIN32)

#include <targetver.h>
#include <Windows.h>

static void delayMs(int ms) { ::Sleep(ms); }

#ifdef _MSC_VER
#pragma comment(lib, "usb_io_interface.lib")
#endif //MSC_VER

#endif //WIN32

#include <cstdio>
#include <tchar.h>

#include <usb_io_device.h>


typedef intptr_t devhnd_t;

///////////////////////////////////////////////////

int enum_test()
{
    printf("Enumerating devices...\n");
    usb_io_device_info *usb_io_list = NULL;
    int n = 0;
    usb_io_list = usb_io_get_device_list();
    if (!usb_io_list) {
        printf("No devices found\n");
        return 1;
    }

    for(usb_io_device_info *tmp = usb_io_list; 
        tmp; 
        tmp = tmp->next ) {
        n++;
        printf("%d: serial number:%s\n", n, tmp->serial_number);
    }

    printf("Found %d devices\n", n);
    usb_io_free_device_list(usb_io_list);
    return 0;
}

///////////////////////////////////////////////////
// Wrapper for calling test on 1st found device
int one_dev_test( int (*ftest)(devhnd_t dev) )
{
    int rc = -1;
    usb_io_device_info *usb_io_list = NULL;
    usb_io_list = usb_io_get_device_list();
    if (!usb_io_list) {
        printf("No devices found\n");
        return rc;
    }

    for(struct usb_io_device_info *tmp = usb_io_list; 
        tmp; 
        tmp = tmp->next ) {
        printf("Running on device with serial number:%s\n", tmp->serial_number);
        
        if (ftest) {
          devhnd_t hnd = usb_io_open_device(usb_io_list);
          try {
              rc = ftest( hnd );
          } catch(...) {
              printf("\nException!\n");
          }
          usb_io_close_device(hnd);
        }
        
        break; //only 1st device
    }

    usb_io_free_device_list(usb_io_list);
    return rc;
}

///////////////////////////////////////////////////

int test_LED_on_off(devhnd_t dev)
{
    if(usb_io_set_work_led_mode(dev, OPEN_WORK_LED) != 0) {
        printf("open work led failure\n");
        return 2;
    }

    delayMs(2500);

    if(usb_io_set_work_led_mode(dev, CLOSE_WORK_LED) != 0) {
        printf("close work led failure\n");
        return 2;
    }

    return 0;
}


///////////////////////////////////////////////////

int orig_loop_test()
{
    while(1)
    {
        //get all device list
        struct usb_io_device_info *usb_io_list = NULL;
        usb_io_list = usb_io_get_device_list();

        //print all device infomation
        printf("****************************\n");
        struct usb_io_device_info *tmp = usb_io_list;
        while(tmp)
        {
            printf("serial number:%s\n", tmp->serial_number);
            tmp = tmp->next;
        }

        if (usb_io_list)
        {
            struct pin_info inof[16] = {0};
            devhnd_t hand = usb_io_open_device(usb_io_list);
            if (hand)
            {
                //1. open work led
                if(usb_io_set_work_led_mode(hand, OPEN_WORK_LED) != 0)
                {
                    printf("open work led failure\n");
                    goto exit;
                }

                printf("all pin output high level\n");
                for (int i = 0; i < 16; i++)
                {
                    //set work mode
                    usb_io_set_pin_mode(hand, i, OUTPUT_MODE, NO_INNNER_PULL_UP);
                    //set out high level
                    usb_io_write_output_pin_value(hand, i, HIGHT_LEVEL);
                }

                delayMs(500);

                printf("all pin output low level\n");
                for (int i = 0; i < 16; i++)
                {
                    usb_io_set_pin_mode(hand, i, OUTPUT_MODE, NO_INNNER_PULL_UP);
                    usb_io_write_output_pin_value(hand, i, LOW_LEVEL);
                }

                delayMs(500);

            }
            else
            {
                printf("open device error...!\n");
            }
            usb_io_close_device(hand);
        }
        else
        {
            printf("find no USB IO device\n");
        }

        usb_io_free_device_list(usb_io_list);
    }

    exit:
    return 0;
}


int main(int argc, char* argv[])
{
    int rc = 0;
    usb_io_init();

    try {

//    rc = orig_loop_test();
//    rc = enum_test();
      rc = one_dev_test( test_LED_on_off );

    } catch(...) {
        printf("\nException!\n");
    }

    delayMs(100);
    usb_io_uninit();

    return rc;
}
