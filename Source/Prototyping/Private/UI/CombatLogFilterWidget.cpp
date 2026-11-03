#include "UI/CombatLogFilterWidget.h"

void UCombatLogFilterWidget::Setup(ECombatLogCategory Category, const FText& Label, const FLinearColor& Color, bool bChecked)
{
	Channel = Category;
	if (FilterLabel_Text)
	{
		FilterLabel_Text->SetText(Label);
		FilterLabel_Text->SetColorAndOpacity(FSlateColor(Color));
	}
	if (FilterIcon)
		FilterIcon->SetColorAndOpacity(Color);
	if (FilterCheckBox)
	{
		FilterCheckBox->SetIsChecked(bChecked);
		FilterCheckBox->OnCheckStateChanged.AddDynamic(this, &UCombatLogFilterWidget::HandleCheckChanged);
	}
}

bool UCombatLogFilterWidget::IsChannelChecked() const
{
	// No checkbox bound = no way to turn off.
	return !FilterCheckBox || FilterCheckBox->IsChecked();
}

void UCombatLogFilterWidget::SetChannelChecked(bool bChecked)
{
	if (FilterCheckBox)
		FilterCheckBox->SetIsChecked(bChecked);
}

void UCombatLogFilterWidget::HandleCheckChanged(bool /*bIsChecked*/)
{
	OnFilterToggled.Broadcast();
}
