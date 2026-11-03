#include "UI/CombatLogRowWidget.h"

void UCombatLogRowWidget::SetRowData(ECombatLogCategory Category, const FText& Time, const FText& Body, const FLinearColor& Color)
{
	RowCategory = Category;
	if (Row_Time_Text)
		Row_Time_Text->SetText(Time);
	if (Row_Body_Text)
	{
		Row_Body_Text->SetText(Body);
		Row_Body_Text->SetColorAndOpacity(FSlateColor(Color));
	}
	if (Row_Icon)
		Row_Icon->SetColorAndOpacity(Color);
}
