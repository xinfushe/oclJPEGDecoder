#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <cl\opencl.h>

#include "macro.h"
#include "idct.h"

const size_t BLOCK_SIZE=sizeof(int)*64;

cl_device_id sel_device;
cl_context g_context;
cl_command_queue g_commandq;
cl_mem g_block_data;
int g_block_count;

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
                    // choose high-performace gpu automatically
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
        printf("[X] No suitable OpenCL device. A Quadro or FirePro graphics card would be a good choice.\n");
        return 1;
    }
}

bool clidct_create()
{
    cl_int err;
    g_context=clCreateContext(NULL,1,&sel_device,NULL,NULL,&err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clCreateContext failed (error %d)\n", err);
        return false;
    }
    g_commandq=clCreateCommandQueue(g_context,sel_device,0,&err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clCreateCommandQueue failed (error %d)\n", err);
        return false;
    }
    return true;
}

bool clidct_allocate_memory(const int total_blocks, const int image_width, const int image_height)
{
    cl_int err;
    g_block_data=clCreateBuffer(g_context,CL_MEM_READ_WRITE,BLOCK_SIZE*total_blocks,NULL,&err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clCreateBuffer failed (error %d)\n", err);
        return false;
    }
    g_block_count=total_blocks;
    return true;
}

bool clidct_transfer_data_to_device(const int block_data_src[0][64], const int offset, const int count)
{
    assert(offset+count<=g_block_count);
    cl_int err;
    size_t write_size;
    write_size=BLOCK_SIZE*count;
    err=clEnqueueWriteBuffer(g_commandq,g_block_data,CL_TRUE,0,write_size,block_data_src,0,NULL,NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clEnqueueWriteBuffer failed (error %d)\n", err);
        return false;
    }else
    {
        printf("[ ] Writing %u bytes to device...\n",write_size);
    }
    clFinish(g_commandq);
    return true;
}

bool clidct_ready()
{
    // TODO: create program
    return false;
}

bool clidct_run()
{
    return false;
}

bool clidct_wait_for_completion()
{
    return false;
}

bool clidct_retrieve_data_from_device(int* mcu_data_dest)
{
    return false;
}

bool clidct_clean_up()
{
    if (g_block_data)
    {
        clReleaseMemObject(g_block_data);
        g_block_data=0;
        g_block_count=0;
    }
    if (g_commandq)
    {
        clReleaseCommandQueue(g_commandq);
        g_commandq=0;
    }
    if (g_context)
    {
        clReleaseContext(g_context);
        g_context=0;
    }
    return true;
}
