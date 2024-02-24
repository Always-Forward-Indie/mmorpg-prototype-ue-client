// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/PingManager.h"

UPingManager::UPingManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	

}

void UPingManager::Initialize(UNetworkManager* NetworkManager, UMonitorStatsWidget* MonitorStats)
{
	// Initialize the network manager
	networkManager = NetworkManager;

	// Initialize the monitor stats widget
	MonitorStatsWidget = MonitorStats;
}

// set the world context
void UPingManager::SetWorldContext(UWorld* World)
{
	worldContext = World; // Set the world context
}

void UPingManager::CalculatePingTime(const TArray<FDateTime>& SendTimes, const TArray<FDateTime>& ReceiveTimes, const FString& serverName)
{
    // Ensure both arrays have at least one element
    if (SendTimes.Num() == 0 || ReceiveTimes.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("SendTimes or ReceiveTimes array is empty!"));
        return;
    }

    // Determine the minimum length between SendTimes and ReceiveTimes arrays
    int32 MinLength = FMath::Min(SendTimes.Num(), ReceiveTimes.Num());

    // Calculate ping time for each pair of SendTime and ReceiveTime
    TArray<float> PingTimes;
    for (int32 i = 0; i < MinLength; ++i)
    {
        float PingTime = (ReceiveTimes[i] - SendTimes[i]).GetTotalMilliseconds();
        if (PingTime >= 0) // Skip negative ping times
        {
            PingTimes.Add(PingTime);
        }
    }

    // Calculate the average ping time
    float TotalPingTime = 0.0f;
    int32 NumValidPings = 0; // Counter for valid ping times
    for (float PingTime : PingTimes)
    {
        TotalPingTime += PingTime;
        NumValidPings++; // Increment the counter for each valid ping time
    }

    // Ensure at least one valid ping time before calculating the average
    if (NumValidPings > 0)
    {
        // Calculate mean and standard deviation
        float MeanPingTime = TotalPingTime / NumValidPings;
        float Variance = 0.0f;
        for (float PingTime : PingTimes)
        {
            Variance += FMath::Square(PingTime - MeanPingTime);
        }
        Variance /= NumValidPings;
        float StdDev = FMath::Sqrt(Variance);

        // Filter out outliers based on z-score threshold
        const float ZScoreThreshold = 3.0f; // Adjust as needed
        TArray<float> FilteredPingTimes;
        for (float PingTime : PingTimes)
        {
            float ZScore = (PingTime - MeanPingTime) / StdDev;
            if (FMath::Abs(ZScore) <= ZScoreThreshold)
            {
                FilteredPingTimes.Add(PingTime);
            }
        }

        // Recalculate total ping time and number of valid pings
        TotalPingTime = 0.0f;
        NumValidPings = 0;
        for (float PingTime : FilteredPingTimes)
        {
            TotalPingTime += PingTime;
            NumValidPings++;
        }

        // Calculate the average ping time
        float AveragePingTime = TotalPingTime / NumValidPings;

        // Log the average ping time
        UE_LOG(LogTemp, Warning, TEXT("Average ping time for %s: %.2f ms"), *serverName, AveragePingTime);

        // Set the average ping time to the monitor widget (assuming MonitorStatsWidget is valid)
        if (MonitorStatsWidget != nullptr)
        {
            if (serverName == "Login Server")
            {
                MonitorStatsWidget->SetLoginServerPingValue(FString::Printf(TEXT("%.2f ms"), AveragePingTime));
            }
            else if (serverName == "Game Server")
            {
                MonitorStatsWidget->SetGameServerPingValue(FString::Printf(TEXT("%.2f ms"), AveragePingTime));
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No valid ping times found for %s."), *serverName);
    }
}
