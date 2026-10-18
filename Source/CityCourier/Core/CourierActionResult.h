#pragma once
#include "CoreMinimal.h"

struct FCourierActionResult
{
    bool bSucceeded = false;
    FText Reason;
    static FCourierActionResult Success() { return {true, FText::GetEmpty()}; }
    static FCourierActionResult Failure(const TCHAR* Message) { return {false, FText::FromString(Message)}; }
};
