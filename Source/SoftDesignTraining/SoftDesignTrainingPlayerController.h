// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GameFramework/PlayerController.h"
#include "SoftDesignTrainingPlayerController.generated.h"

UCLASS()
class ASoftDesignTrainingPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASoftDesignTrainingPlayerController();

protected:

    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;

    void MoveForward(float axisValue);
    void MoveRight(float axisValue);

};


