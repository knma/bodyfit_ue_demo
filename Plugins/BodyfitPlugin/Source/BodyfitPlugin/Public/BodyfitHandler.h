// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "Json.h"
#include "JsonUtilities.h"

#include "BodyfitHandler.generated.h"


USTRUCT(BlueprintType)
struct BODYFITPLUGIN_API FBodyProcessingResponse {
	GENERATED_USTRUCT_BODY()
	UPROPERTY() FString uid;
	UPROPERTY() FString error;
	FBodyProcessingResponse() {}
};

USTRUCT(BlueprintType)
struct BODYFITPLUGIN_API FBodyProcessingResultResponse {
	GENERATED_USTRUCT_BODY()
	UPROPERTY() FString uid;
	UPROPERTY() FString error;
	FBodyProcessingResultResponse() {}
};

USTRUCT(BlueprintType)
struct BODYFITPLUGIN_API FBodyLoopConvexEdge {
	GENERATED_USTRUCT_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") TArray<FVector2D> Points;
	FBodyLoopConvexEdge() {}
};

USTRUCT(BlueprintType)
struct BODYFITPLUGIN_API FBodyLoopStep {
	GENERATED_USTRUCT_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") FVector StepPoint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") TArray<FBodyLoopConvexEdge> EdgePoints;		// 2D convex hull contour
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") FVector LeftmostPoint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") FVector RightmostPoint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") FVector Direction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") float Length;
	FBodyLoopStep() {}
};

USTRUCT(BlueprintType)
struct BODYFITPLUGIN_API FBodyLoop {
	GENERATED_USTRUCT_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") TArray<FBodyLoopStep> Steps;
	FBodyLoop() {}
};

USTRUCT(BlueprintType)
struct BODYFITPLUGIN_API FBodyProcessingResult {
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") TArray<FString> PreviewFramesURL;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") FString PreviewVideoURL;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") FString ModelURL;							// .obj model

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") TArray<FVector> Pose;						// (24, 3) axis-angle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") TArray<FVector> Joints;					// (24, 3) global space
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") TArray<float> Shape;						// (10,) SMPL shape 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") TMap<FString, float> LinearMeasurements;	// spine, hands, legs length
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit") TMap<FString, FBodyLoop> Loops;			// 5 steps for each loop type

	FVector GetVectorFromJSONList(const TArray<TSharedPtr<FJsonValue>>& Vals)
	{
		return FVector(
			FCString::Atof(*Vals[0]->AsString()),
			FCString::Atof(*Vals[1]->AsString()),
			FCString::Atof(*Vals[2]->AsString())
		);
	}

	FVector2D GetVector2DFromJSONList(const TArray<TSharedPtr<FJsonValue>>& Vals)
	{
		return FVector2D(
			FCString::Atof(*Vals[0]->AsString()),
			FCString::Atof(*Vals[1]->AsString())
		);
	}

	void FromJSON(const TSharedPtr<FJsonObject>& ResultJSON)
	{
		Clear();

		PreviewVideoURL = ResultJSON->GetStringField("video");
		ModelURL = ResultJSON->GetStringField("model");
		TArray<TSharedPtr<FJsonValue>> frames = ResultJSON->GetArrayField("frames");
		for (int i = 0; i < frames.Num(); i++)
		{
			PreviewFramesURL.Add(frames[i]->AsString());
		}
		TArray<TSharedPtr<FJsonValue>> pose = ResultJSON->GetArrayField("pose");
		for (int i = 0; i < pose.Num(); i++)
		{
			TArray<TSharedPtr<FJsonValue>> ipose = pose[i]->AsArray();
			Pose.Add(GetVectorFromJSONList(ipose));
		}
		TArray<TSharedPtr<FJsonValue>> joints = ResultJSON->GetArrayField("joints");
		for (int i = 0; i < joints.Num(); i++)
		{
			TArray<TSharedPtr<FJsonValue>> ijoints = joints[i]->AsArray();
			Joints.Add(GetVectorFromJSONList(ijoints));
		}
		TArray<TSharedPtr<FJsonValue>> shape = ResultJSON->GetArrayField("shape");
		for (int i = 0; i < shape.Num(); i++)
		{
			Shape.Add(FCString::Atof(*shape[i]->AsString()));
		}
		for (const auto &lm : ResultJSON->GetObjectField("linear_measurements")->Values)
		{
			LinearMeasurements.Add(lm.Key, lm.Value->AsNumber());
		}
		for (const auto& loop : ResultJSON->GetObjectField("loops")->Values)
		{
			FBodyLoop BodyLoop;
			TArray<TSharedPtr<FJsonValue>> loop_steps = loop.Value->AsArray();
			for (int i = 0; i < loop_steps.Num(); i++)
			{
				FBodyLoopStep BodyLoopStep;
				TSharedPtr<FJsonObject> loop_step = loop_steps[i]->AsObject();

				BodyLoopStep.StepPoint = GetVectorFromJSONList(loop_step->GetArrayField("step_point"));
				BodyLoopStep.Direction = GetVectorFromJSONList(loop_step->GetArrayField("direction"));
				BodyLoopStep.LeftmostPoint = GetVectorFromJSONList(loop_step->GetArrayField("leftmost_point"));
				BodyLoopStep.RightmostPoint = GetVectorFromJSONList(loop_step->GetArrayField("rightmost_point"));
				BodyLoopStep.Length = FCString::Atof(*loop_step->GetStringField("length"));

				auto& edges = loop_step->GetArrayField("edge_points");
				for (int j = 0; j < edges.Num(); j++)
				{
					FBodyLoopConvexEdge Edge;
					auto& edge_points = edges[j]->AsArray();
					for (int k = 0; k < edge_points.Num(); k++)
					{
						Edge.Points.Add(GetVector2DFromJSONList(edge_points[k]->AsArray()));
					}
					BodyLoopStep.EdgePoints.Add(Edge);
				}

				BodyLoop.Steps.Add(BodyLoopStep);
			}

			Loops.Add(loop.Key, BodyLoop);
		}
	}

	void Clear()
	{
		PreviewFramesURL.Empty();
		Pose.Empty();
		Joints.Empty();
		Shape.Empty();
		LinearMeasurements.Empty();
		Loops.Empty();
	}

	void DebugPrint()
	{
		if (PreviewFramesURL.Num()) UE_LOG(LogTemp, Warning, TEXT("PreviewFramesURL %s"), *PreviewFramesURL[0]);
		if (Pose.Num()) UE_LOG(LogTemp, Warning, TEXT("Pose %s"), *Pose[0].ToString());
		if (Joints.Num()) UE_LOG(LogTemp, Warning, TEXT("Joints %s"), *Joints[0].ToString());
	}

	FBodyProcessingResult() {}
};


UCLASS()
class BODYFITPLUGIN_API ABodyfitHandler : public AActor
{
	GENERATED_BODY()
	
public:	
	ABodyfitHandler();

	UFUNCTION(BlueprintCallable, Category = "Bodyfit")
	bool ProcessBody();

	UFUNCTION(BlueprintNativeEvent, Category = "Bodyfit")
	void ResultReceived();

	UFUNCTION(BlueprintNativeEvent, Category = "Bodyfit")
	void ResultPollingTimedOut();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit")
	FString URL = "http://localhost:5000";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit")
	FString Gender = "male";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit")
	FString FrontImg = "C:\\Users\\knma\\dev\\body_measurements\\body\\test_data\\m_03_front.jpg";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit")
	FString SideImg = "C:\\Users\\knma\\dev\\body_measurements\\body\\test_data\\m_03_side.jpg";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodyfit")
	float Height = 1.75;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodyfit")
	FBodyProcessingResult Result;

private:
	UFUNCTION()
	void GetResult();

	void StartPolling();
	void OnProcessBodyResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnResultResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString uid;
	FTimerHandle PollingTimerHandle;
	FTimerDelegate PollingTimerDelegate;
	uint32 PollingCounter = 0;
	uint32 PollingTimesLimit = 200;
	float PollingInterval = 3.0f;
	FString InProgressURL;

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

};