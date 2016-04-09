#include "stdafx.h"
#include <CL/opencl.h>

#include "macro.h"
#include "idct.h"

const size_t BLOCK_SIZE=sizeof(int)*64;
const size_t WORK_SIZE[]={512};
const cl_image_format IMG_FORMAT={CL_BGRA, CL_UNSIGNED_INT8};

static cl_device_id sel_device;
static cl_context g_context;
static cl_command_queue g_commandq;
static cl_program g_program;
static cl_kernel g_entry;
static cl_mem g_block_data;
static cl_mem g_image_data; // for output image
static int g_block_count;
static size_t g_image_width;
static size_t g_image_height;
static size_t g_image_pitch;
static int g_num_hor_mcu;
static int g_num_ver_mcu;

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
                if (strstr(dname, "Quadro") || strstr(dname, "Tesla") || strstr(dname, "GeForce") || strstr(dname, "FirePro") || strstr(dname, "FireStream") || strstr(dname, "Capeverde"))
                {
                    // choose high-performace gpu automatically
                    sel_device=did;
                }
                clGetDeviceInfo(did, CL_DEVICE_VERSION, sizeof(dname), dname, NULL);
                printf("\tVersion: %s\n", dname);

                cl_uint max_cu;
                clGetDeviceInfo(did, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(max_cu), &max_cu, NULL);
                printf("\tCompute Units: %u\n",max_cu);

                cl_ulong local_mem_size;
                clGetDeviceInfo(did, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &local_mem_size, 0);
                printf("\tLocal Memory Size: %I64u\n",local_mem_size);

                cl_ulong const_mem_size;
                clGetDeviceInfo(did, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &const_mem_size, 0);
                printf("\tConstant Memory Size: %I64u\n",const_mem_size);
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
    if (err != CL_SUCCESS)
    {
        fprintf(stderr, "clCreateCommandQueue failed (error %d)\n", err);
        return false;
    }
    return true;
}

bool clidct_allocate_memory(const int total_blocks, const size_t image_width, const size_t image_height, const int mcu_width, const int mcu_height)
{
    cl_int err;
    // create dct coefficient blocks buffer
    g_block_data=clCreateBuffer(g_context,CL_MEM_READ_WRITE,BLOCK_SIZE*total_blocks,NULL,&err);
    if (err != CL_SUCCESS)
    {
        fprintf(stderr, "clCreateBuffer failed (error %d)\n", err);
        return false;
    }
    else
    {
        g_block_count=total_blocks;
    }
    // create output image
    const int allocated_width=(image_width+mcu_width-1)&(~(mcu_width-1));
    const int allocated_height=(image_height+mcu_height-1)&(~(mcu_height-1));
    g_image_data=clCreateImage2D(g_context,CL_MEM_WRITE_ONLY,&IMG_FORMAT,allocated_width,allocated_height,0,NULL,&err);
    if (err != CL_SUCCESS)
    {
        fprintf(stderr, "clCreateImage2D failed (error %d)\n", err);
        return false;
    }
    else
    {
        g_image_width=allocated_width;
        g_image_height=allocated_height;
        g_image_pitch=allocated_width*4;
        g_num_hor_mcu=allocated_width/mcu_width;
        g_num_ver_mcu=allocated_height/mcu_height;
    }
    return true;
}

bool clidct_transfer_data_to_device(const int block_data_src[1][64], const int offset, const int count)
{
    assert(offset+count<=g_block_count);
    cl_int err;
    size_t write_size=0;
    // enqueue transfering dct blocks
    write_size+=BLOCK_SIZE*count;
    err=clEnqueueWriteBuffer(g_commandq,g_block_data,CL_TRUE,0,write_size,block_data_src,0,NULL,NULL);
    // send
    if (err != CL_SUCCESS)
    {
        fprintf(stderr, "clEnqueueWriteBuffer failed (error %d)\n", err);
        return false;
    }else
    {
        printf("[ ] Writing %u bytes to device...\n",write_size);
    }
    clFinish(g_commandq);
    return true;
}

bool clidct_retrieve_data_from_device(int block_data_dest[1][64])
{
    cl_int err;
    size_t read_size=0;
    // enqueue transfering dct blocks
    read_size+=BLOCK_SIZE*g_block_count;
    err=clEnqueueReadBuffer(g_commandq,g_block_data,CL_TRUE,0,read_size,block_data_dest,0,NULL,NULL);
    // recv
    if (err != CL_SUCCESS)
    {
        fprintf(stderr, "clEnqueueReadBuffer failed (error %d)\n", err);
        return false;
    }else
    {
        printf("[ ] Retrieving %u bytes from device...\n",read_size);
    }
    clFinish(g_commandq);
    return true;
}

bool clidct_retrieve_image_from_device(void *img_data_dest, const size_t img_width, const size_t img_height)
{
    assert(img_width<=g_image_width && img_height<=g_image_height);
    cl_int err;
    size_t read_size=0;
    // enqueue transfering image
    read_size+=img_width*4*img_height;
    size_t origin[3]={0,0,0};
    size_t region[3]={img_width,img_height,1};
    err=clEnqueueReadImage(g_commandq,g_image_data,CL_TRUE,origin,region,img_width*4,0,img_data_dest,0,NULL,NULL);
    // recv
    if (err != CL_SUCCESS)
    {
        fprintf(stderr, "clEnqueueReadImage failed (error %d)\n", err);
        return false;
    }else
    {
        printf("[ ] Retrieving %u bytes from device...\n",read_size);
    }
    clFinish(g_commandq);
    return true;
}

bool clidct_build(ColorSpace colorspace)
{
    const char *kernel_name=NULL, *code_file=NULL;
    switch (colorspace)
    {
    case YUV444:
        code_file="idct8x8.cl";
        kernel_name="batch_idct_csc_444";
        break;
    case YUV411:
        code_file="idct8x8.cl";
        kernel_name="batch_idct_csc_411";
        break;
    case Other:
        code_file="idct8x8.cl";
        kernel_name="batch_idct"; // run IDCT only
        break;
    }
    FILE *src=fopen(code_file,"rb");
    if (src!=NULL)
    {
        // get the file size
        fseek(src,0,SEEK_END);
        long len=ftell(src);
        rewind(src);

        char* code=new char[len+1];
        code[len]=0;
        int ret=fread(code,len,1,src);
        fclose(src);
        if (ret==1)
        {
            const char* code_str=code;
            size_t code_len=len;
            cl_int err;
            g_program=clCreateProgramWithSource(g_context,1,&code_str,&code_len,&err);
            ret=0;
            if (err==CL_SUCCESS)
            {
                err=clBuildProgram(g_program,1,&sel_device,"-Werror",NULL,NULL);
                if (err==CL_SUCCESS)
                {
                    g_entry=clCreateKernel(g_program,kernel_name,&err);
                    if (err==CL_SUCCESS)
                    {
                        ret=1;
                    }else
                        fprintf(stderr, "clCreateKernel failed (error %d)\n", err);
                }else
                {
                    size_t length;
                    static char buffer[20480];
                    clGetProgramBuildInfo(g_program, sel_device, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &length);
                    fprintf(stderr, "clBuildProgram failed (error %d %s)\n", err, buffer);
                }
            }
            else
                fprintf(stderr, "clCreateProgramWithSource failed (error %d)\n", err);
        }
        delete []code;
        return ret==1;
    }
    return false;
}

bool clidct_run(ColorSpace colorspace)
{
    cl_int err;
    // set execution arguments
    err=clSetKernelArg(g_entry,0,sizeof(cl_mem),&g_block_data);
    err|=clSetKernelArg(g_entry,1,sizeof(int),&g_block_count);
    if (colorspace!=Other)
    {
        // if the colorspace is known, perform color space conversion on GPU and we have image memory in VRAM
        err|=clSetKernelArg(g_entry,2,sizeof(cl_mem),&g_image_data);
        err|=clSetKernelArg(g_entry,3,sizeof(int),&g_num_hor_mcu);
    }
    if (err!=CL_SUCCESS)
    {
        fprintf(stderr, "clSetKernelArg failed (error %d)\n", err);
        return false;
    }
    err=clEnqueueNDRangeKernel(g_commandq,g_entry,COUNT_OF(WORK_SIZE),NULL,&WORK_SIZE[0],NULL,0,NULL,NULL);
    if (err!=CL_SUCCESS)
    {
        fprintf(stderr, "clEnqueueNDRangeKernel failed (error %d)\n", err);
        return false;
    }
    return true;
}

bool clidct_wait_for_completion()
{
    return CL_SUCCESS==clFinish(g_commandq);
}

bool clidct_clean_up()
{
    if (g_entry)
    {
        clReleaseKernel(g_entry);
        g_entry=0;
    }
    if (g_program)
    {
        clReleaseProgram(g_program);
        g_program=0;
    }
    if (g_image_data)
    {
        clReleaseMemObject(g_image_data);
        g_image_data=0;
        g_image_pitch=0;
    }
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
