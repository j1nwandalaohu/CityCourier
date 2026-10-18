#include "Validation/PackagedValidationDriver.h"
#if !UE_BUILD_SHIPPING
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Player/CourierActionComponent.h"
#include "Player/CourierActionAnimation.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"

void APackagedValidationDriver::RunCharacterActionChecks(double Now)
{
    if(Now-StartTime<7) return;
    auto* A=Rider->GetActions(); auto* Movement=Rider->GetCharacterMovement();
    const double Wait=Now-FeatureWaitStart;
    switch(InterfaceCheck)
    {
    case 0:
        PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
        FeatureWorldTime=Rider->GetActorLocation().Z;
        if(!Rider->TryJumpWithFeedback().bSucceeded || !Movement->IsFalling() || Movement->Velocity.Z<400)
        { Finish(TEXT("Real physics jump did not begin")); return; }
        break;
    case 1:
        if(Wait<.12) return;
        if(Rider->GetActorLocation().Z<FeatureWorldTime+10) { Finish(TEXT("Character did not actually rise")); return; }
        Capture(TEXT("physics-jump-pending-art"));
        break;
    case 2:
        if(Wait<.10) return; // Let the jump screenshot render before showing the phone.
        FeatureWorldTime=Movement->Velocity.Z;
        PC->SetInterfaceMode(ECourierInterfaceMode::Phone);
        if(FMath::Abs(Movement->Velocity.Z-FeatureWorldTime)>1) { Finish(TEXT("Phone erased vertical velocity")); return; }
        break;
    case 3:
        if(Wait<.12) return;
        if(Movement->Velocity.Z>=FeatureWorldTime-20) { Finish(TEXT("Gravity did not continue with phone open")); return; }
        PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
        break;
    case 4:
        if(Wait<1) return;
        if(!Movement->IsMovingOnGround()) { Finish(TEXT("Jump failed to land on actual collision")); return; }
        if(!A->HasAttackAnimation(false) || !A->HasAttackAnimation(true))
        {
            if(A->QueueAttack(false) || A->QueueAttack(true) || PC->GetActionFeedback().IsEmpty())
            { Finish(TEXT("Missing animation did not safely reject invisible attack")); return; }
        }
        else if(!A->QueueAttack(false)) { Finish(TEXT("Delivered attack animation failed admission")); return; }
        break;
    case 5:
        if(Wait<.15) return; // HUD feedback refreshes on its own .1-second timer.
        Capture(TEXT("missing-action-art-feedback"));
        break;
    case 6:
        if(Wait<.95) return;
        if(A->GetTimeline().GetAction()!=ECourierAction::None) { Finish(TEXT("Action lock remained after recovery")); return; }
        PC->SetInterfaceMode(ECourierInterfaceMode::Pause); FeatureWorldTime=GetWorld()->GetTimeSeconds();
        break;
    case 7:
        if(Wait<.15) return;
        if(GetWorld()->GetTimeSeconds()!=FeatureWorldTime || A->QueueAttack(false))
        { Finish(TEXT("Pause failed to isolate action input/time")); return; }
        PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
        break;
    default:
    {
        if(Wait<.2) return;
        if(A->GetTimeline().GetAction()!=ECourierAction::None) { Finish(TEXT("UI input leaked into gameplay")); return; }
        FString Missing; int32 Count=0;
        for(FName Name:{FName(TEXT("A_JumpStart")),FName(TEXT("A_JumpLoop")),FName(TEXT("A_Land")),FName(TEXT("A_LightPunch_L")),FName(TEXT("A_LightPunch_R")),
            FName(TEXT("A_HeavyPunch_R")),FName(TEXT("A_HitReact_Front")),FName(TEXT("A_HitReact_Back")),FName(TEXT("A_RunStart")),FName(TEXT("A_RunStop"))})
            if(!FCourierActionAnimation::Load(Name)) { if(Count++) Missing+=TEXT(","); Missing+=TEXT("\"")+Name.ToString()+TEXT("\""); }
        const FString Report=FString::Printf(TEXT("{\"physics_jump\":true,\"airborne_ui_gravity\":true,\"pause_and_menu_isolation\":true,\"missing_asset_count\":%d,\"pending_art\":[%s],\"new_animation_visual_acceptance\":false}"),Count,*Missing);
        FFileHelper::SaveStringToFile(Report,*(Output/TEXT("actions.json")));
        Finish(); return;
    }
    }
    FeatureWaitStart=Now; ++InterfaceCheck;
}
#endif
