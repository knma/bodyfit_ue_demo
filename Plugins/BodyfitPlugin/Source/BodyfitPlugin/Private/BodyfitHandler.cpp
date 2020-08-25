
#include "BodyfitHandler.h"

#include "Async/Async.h"

ABodyfitHandler::ABodyfitHandler()
{
	PrimaryActorTick.bCanEverTick = true;

}

void ABodyfitHandler::BeginPlay()
{
	Super::BeginPlay();

	PollingTimerDelegate.Unbind();
}

void ABodyfitHandler::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool ABodyfitHandler::ProcessBody()
{
	PollingTimerDelegate.Unbind();
	GetWorldTimerManager().ClearTimer(PollingTimerHandle);

	AsyncTask(ENamedThreads::GameThread, [=](){
		TArray<uint8> Content, FrontImgData, SideImgData;
		FFileHelper::LoadFileToArray(FrontImgData, *FrontImg);
		if (SideImg.Len())
		{
			FFileHelper::LoadFileToArray(SideImgData, *SideImg);
		}
		

		FString BoundaryLabel = FString(TEXT("e543322540af456f9a3773049ca02529-")) + FString::FromInt(FMath::Rand());
		FString BoundaryBegin = FString(TEXT("\r\n--")) + BoundaryLabel + FString(TEXT("\r\n"));
		FString BoundaryEnd = FString(TEXT("\r\n--")) + BoundaryLabel + FString(TEXT("--\r\n"));

		TSharedRef<IHttpRequest> Request = (&FHttpModule::Get())->CreateRequest();
		Request->OnProcessRequestComplete().BindUObject(this, &ABodyfitHandler::OnProcessBodyResponseReceived);
		Request->SetHeader(TEXT("Content-Type"), FString(TEXT("multipart/form-data; boundary=")) + BoundaryLabel);

		{
			FString Data = BoundaryBegin;
			Data += FString(TEXT("Content-Disposition: form-data; name=\"query\"\r\n"));
			Data += FString(TEXT("Content-Type: application/json\r\n\r\n"));
			Data += FString("{");
			Data += FString("\"gender\": \"") + Gender + FString("\", \"height\": \"") + FString::SanitizeFloat(Height) + FString("\",");
			Data += FString("\"front_coarse_only\": \"") + FString::FromInt(int(FrontCoarseMode)) + FString("\", \"use_spin\": \"") + FString::FromInt(int(SpinMode)) + FString("\"");
			Data += FString("}");
			FTCHARToUTF8 Converted(*Data);
			Content.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
		}

		auto AddImage = [&](TArray<uint8>& ImgData, FString ImgName, FString ImgFileName) {
			FString ContentType = ImgFileName.Contains(TEXT(".png")) ? TEXT("image/png") : TEXT("image/jpeg");
			FString Data = BoundaryBegin;
			Data += FString("Content-Disposition: form-data; name=\"") + ImgName + FString("\"; filename=\"") + ImgFileName + FString("\"\r\n");
			Data += FString("Content-Type: ") + ContentType + FString("\r\n\r\n");
			FTCHARToUTF8 Converted(*Data);
			Content.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
			Content.Append(ImgData);
		};

		AddImage(FrontImgData, "front_img", FrontImg);
		if (SideImg.Len())
		{
			AddImage(SideImgData, "side_img", SideImg);
		}

		{
			FTCHARToUTF8 Converted(*(FString(TEXT("\r\n")) + BoundaryEnd));
			Content.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
		}

		InProgressURL = URL;
		Request->SetURL(URL + FString("/process"));
		Request->SetVerb(TEXT("POST"));
		Request->SetContent(Content);

		Request->ProcessRequest();
	});

	return true;
}


void ABodyfitHandler::GetResult()
{
	AsyncTask(ENamedThreads::GameThread, [=]() {
		TSharedRef<IHttpRequest> Request = (&FHttpModule::Get())->CreateRequest();
		Request->OnProcessRequestComplete().BindUObject(this, &ABodyfitHandler::OnResultResponseReceived);
		Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
		Request->SetURL(InProgressURL + FString("/get_result?uid=") + uid);
		Request->SetVerb("GET");
		Request->ProcessRequest();
	});
	UE_LOG(LogTemp, Warning, TEXT("Get Processing Result Request %d"), PollingCounter);

	PollingCounter--;

	if (!PollingCounter) {
		GetWorldTimerManager().ClearTimer(PollingTimerHandle);
		ResultPollingTimedOut();
	}
}

void ABodyfitHandler::OnProcessBodyResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful) {
		UE_LOG(LogTemp, Error, TEXT("Body Processing Request Failed"));
		return;
	}

	FString JsonString = Response->GetContentAsString();

	FBodyProcessingResponse ProcessingResponse;
	FJsonObjectConverter::JsonObjectStringToUStruct<FBodyProcessingResponse>(JsonString, &ProcessingResponse, 0, 0);

	UE_LOG(LogTemp, Warning, TEXT("Processing Response"));
	UE_LOG(LogTemp, Warning, TEXT("uid: %s"), *ProcessingResponse.uid);
	UE_LOG(LogTemp, Warning, TEXT("error: %s"), *ProcessingResponse.error);

	if (ProcessingResponse.uid.Len()) {
		uid = ProcessingResponse.uid;

		PollingTimerDelegate.Unbind();
		PollingTimerDelegate.BindUFunction(this, FName("GetResult"));

		PollingCounter = PollingTimesLimit;
		GetWorldTimerManager().ClearTimer(PollingTimerHandle);
		GetWorldTimerManager().SetTimer(PollingTimerHandle, PollingTimerDelegate, PollingInterval, true);
	}
	else if (ProcessingResponse.error.Len()) {
		UE_LOG(LogTemp, Warning, TEXT("Body Processing: %s"), *ProcessingResponse.error);
	}
}

void ABodyfitHandler::OnResultResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful) {
		UE_LOG(LogTemp, Error, TEXT("Result Request Failed"));
		return;
	}

	FString JsonString = Response->GetContentAsString();

	FBodyProcessingResultResponse ResultResponse;
	FJsonObjectConverter::JsonObjectStringToUStruct<FBodyProcessingResultResponse>(JsonString, &ResultResponse, 0, 0);

	if (!ResultResponse.error.Len()) {
		PollingTimerDelegate.Unbind();
		GetWorldTimerManager().ClearTimer(PollingTimerHandle);

		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Response->GetContentAsString());
		FJsonSerializer::Deserialize(JsonReader, JsonObject);

		Result.FromJSON(JsonObject->GetObjectField("result"));
		Result.DebugPrint();

		ResultReceived();
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Body Processing Result Error: %s"), *ResultResponse.error);
	}

	UE_LOG(LogTemp, Warning, TEXT("uid: %s"), *ResultResponse.uid);
}

void ABodyfitHandler::ResultReceived_Implementation() {}
void ABodyfitHandler::ResultPollingTimedOut_Implementation() {}