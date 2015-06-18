#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <cl\opencl.h>

#include "macro.h"
#include "idct.h"

cl_device_id sel_device;

int Initialize_OpenCL_IDCT()
{
    puts("[ ] Initializing OpenCL Environment");

    cl_platform_id platform_ids[10];
    cl_uint num_platforms;
    cl_int err;

    err = clGetPlatformIDs(COUNT_OF(platform_ids), platform_ids, &num_platforms);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clGetPlatformIDs failed (error %d)\n", err);
        return -1;
    }

    for (cl_uint i = 0; i < num_platforms; i++) {
        const cl_platform_id pid = platform_ids[i];
        printf("Platform #%u:\n", i+1);

        char dname[500];
        clGetPlatformInfo(pid, CL_PLATFORM_NAME, 500, dname, NULL);
        printf("\tName: %s\n", dname);
        clGetPlatformInfo(pid, CL_PLATFORM_VERSION, 500, dname, NULL);
        printf("\tVersion: %s\n", dname);

        cl_device_id dev_ids[10];
        cl_uint num_devices;
        clGetDeviceIDs(pid, CL_DEVICE_TYPE_ALL, COUNT_OF(dev_ids), dev_ids, & num_devices);
        if (err != CL_SUCCESS) {
            fprintf(stderr, "clGetDeviceIDs failed (error %d)\n", err);
            return -1;
        }else {
            for (cl_uint j = 0; j < num_devices; j++) {
                const cl_device_id did = dev_ids[j];

                clGetDeviceInfo(did, CL_DEVICE_NAME, sizeof(dname), dname, NULL);
                printf("Device #%u: Name: %s\n", j+1, dname);
                if (strstr(dname, "Quadro") || strstr(dname, "Tesla") || strstr(dname, "Geforce") || strstr(dname, "FirePro") || strstr(dname, "FireStream"))
                {
                    // choose high-performace gpu automatially
                    sel_device=did;
                }
                clGetDeviceInfo(did, CL_DEVICE_VERSION, sizeof(dname), dname, NULL);
                printf("\tVersion: %s\n", dname);

                cl_uint max_cu;
                clGetDeviceInfo(did, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(max_cu), &max_cu, NULL);
                printf("\tCompute Units: %u\n",max_cu);
            }
        }
    }

    if (sel_device)
    {
        printf("[ ] OpenCL device selected.\n");
        return 0;
    }else
    {
        printf("[X] No suitable OpenCL device selected. A Quadro or FirePro graphics card would be a good choice.\n");
        return 1;
    }
}
