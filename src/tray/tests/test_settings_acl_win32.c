#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <assert.h>

static const wchar_t *descriptor_text;

static DWORD WINAPI query_security(HANDLE handle, SE_OBJECT_TYPE type,
    SECURITY_INFORMATION information, PSID *owner, PSID *group,
    PACL *dacl, PACL *sacl, PSECURITY_DESCRIPTOR *descriptor)
{
    (void)handle;
    (void)information;
    (void)group;
    (void)sacl;
    assert(type == SE_FILE_OBJECT);
    assert(ConvertStringSecurityDescriptorToSecurityDescriptorW(
        descriptor_text, SDDL_REVISION_1, descriptor, NULL));
    BOOL present, defaulted;
    assert(GetSecurityDescriptorOwner(*descriptor, owner, &defaulted));
    assert(GetSecurityDescriptorDacl(*descriptor, &present, dacl, &defaulted));
    assert(present);
    return ERROR_SUCCESS;
}

/* Exercise the file permission check with Windows-parsed descriptors, without
 * needing administrator privileges to replace a real file's owner or DACL. */
#define GetSecurityInfo query_security
#include "../settings_file_win32.c"
#undef GetSecurityInfo

#define TRUSTED_INSTALLER L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464"

int main(void)
{
    const struct {
        const wchar_t *descriptor;
        bool local_service;
        DWORD expected;
    } cases[] = {
        /* Program Files grants TrustedInstaller full control of directories. */
        {L"O:BAG:SYD:(A;;FA;;;" TRUSTED_INSTALLER L")(A;;FA;;;BA)(A;;FR;;;BU)", false, ERROR_SUCCESS},
        {L"O:" TRUSTED_INSTALLER L"G:SYD:(A;;FA;;;SY)", false, ERROR_SUCCESS},
        {L"O:BAG:SYD:(A;;FA;;;SY)(A;;FA;;;BA)(A;;FR;;;BU)", false, ERROR_SUCCESS},
        {L"O:BAG:SYD:(A;;FA;;;BA)(A;;FW;;;LS)(A;;FR;;;BU)", true, ERROR_SUCCESS},
        {L"O:BAG:SYD:(A;;FA;;;BA)(A;;FW;;;LS)", false, ERROR_BAD_CONFIGURATION},
        {L"O:BAG:SYD:(A;;FA;;;BA)(A;;0x116;;;BU)", true, ERROR_BAD_CONFIGURATION},
        {L"O:BAG:SYD:(A;;FA;;;BA)(A;;FW;;;AU)", false, ERROR_BAD_CONFIGURATION},
        {L"O:BUG:SYD:(A;;FA;;;BA)", true, ERROR_BAD_CONFIGURATION}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        descriptor_text = cases[i].descriptor;
        assert(protected_handle(NULL, cases[i].local_service) == cases[i].expected);
    }
    return 0;
}
