
#include <example_error_sac.h>

const char* ReturnCodeName[13] =
{
    "DDS_RETCODE_OK", "DDS_RETCODE_ERROR", "DDS_RETCODE_UNSUPPORTED",
    "DDS_RETCODE_BAD_PARAMETER", "DDS_RETCODE_PRECONDITION_NOT_MET",
    "DDS_RETCODE_OUT_OF_RESOURCES", "DDS_RETCODE_NOT_ENABLED",
    "DDS_RETCODE_IMMUTABLE_POLICY", "DDS_RETCODE_INCONSISTENT_POLICY",
    "DDS_RETCODE_ALREADY_DELETED", "DDS_RETCODE_TIMEOUT", "DDS_RETCODE_NO_DATA",
    "DDS_RETCODE_ILLEGAL_OPERATION"
};

/**
 * Function to convert DDS return codes into an error message on the standard output.
 * @param returnCode DDS return code
 * @param where A string detailing where the error occurred
 */
void checkStatus(DDS_ReturnCode_t returnCode, const char *where)
{
    if (returnCode && returnCode != DDS_RETCODE_NO_DATA)
    {
        fprintf(stderr, "%s%s\n", ReturnCodeName[returnCode], where);
        exit(1);
    }
}

/**
 * Function to check for a null handle and display an error message on the standard output.
 * @param Handle to check for null
 * @param A string detailing where the error occured
 */
void checkHandle(void *handle, const char *where)
{
    if (!handle)
    {
        fprintf(stderr, "%s\n", where);
        exit(1);
    }
}
