#include "logger.hpp"

VOID DriverUnload(PDRIVER_OBJECT drvObject)
{
	DBG_PRINT("unload point called");
	Logger::DeleteRoutine();
}

NTSTATUS DriverEntry(PDRIVER_OBJECT drvObject, PUNICODE_STRING regPath)
{
	DBG_PRINT("entry point called");
	drvObject->DriverUnload = DriverUnload;
	return Logger::InstallRoutine(drvObject);
}