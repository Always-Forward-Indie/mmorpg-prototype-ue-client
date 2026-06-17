#include "Gameplay/UI/GameVersionWidget.h"
#include "MyGameInstance.h"

void UGameVersionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (VersionText)
	{
		UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
		if (GI)
		{
			VersionText->SetText(FText::FromString(GI->ClientVersion));
		}
		else
		{
			VersionText->SetText(FText::FromString(TEXT("?.?.?")));
		}
	}
}
