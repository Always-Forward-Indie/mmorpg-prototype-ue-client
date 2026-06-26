#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Data/DataStructs.h"
#include "SkillTooltipWidget.generated.h"

/**
 * Tooltip widget that displays detailed skill information when hovering over skill items
 */
UCLASS()
class PROTOTYPING_API USkillTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USkillTooltipWidget(const FObjectInitializer& ObjectInitializer);

	// Set skill data to display in tooltip
	UFUNCTION(BlueprintCallable, Category = "Skill Tooltip")
	void SetSkillData(const FPlayerSkillData& SkillData);

	// Update tooltip position
	UFUNCTION(BlueprintCallable, Category = "Skill Tooltip")
	void UpdateTooltipPosition(FVector2D ScreenPosition);

	// Show tooltip with animation
	UFUNCTION(BlueprintCallable, Category = "Skill Tooltip")
	void ShowTooltip();

	// Hide tooltip with animation
	UFUNCTION(BlueprintCallable, Category = "Skill Tooltip")
	void HideTooltip();

protected:
	virtual void NativeConstruct() override;

	// Update all tooltip content
	UFUNCTION(BlueprintCallable, Category = "Skill Tooltip")
	void UpdateTooltipContent();

	// Update skill name and school
	UFUNCTION(BlueprintCallable, Category = "Skill Tooltip")
	void UpdateSkillHeader();

	// Update skill description
	UFUNCTION(BlueprintCallable, Category = "Skill Tooltip")
	void UpdateSkillDescription();

	// Update skill stats (damage, cost, cooldown, etc.)
	UFUNCTION(BlueprintCallable, Category = "Skill Tooltip")
	void UpdateSkillStats();

	// Update skill level and requirements
	UFUNCTION(BlueprintCallable, Category = "Skill Tooltip")
	void UpdateSkillLevelInfo();

	// Get school color
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Tooltip")
	FLinearColor GetSchoolColor(ESkillSchool School) const;

	// Get effect type color
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Tooltip")
	FLinearColor GetEffectTypeColor(ESkillEffectType EffectType) const;

	// Format cooldown text
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Tooltip")
	FText FormatCooldownText(float CooldownMs) const;

	// Format mana cost text
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Tooltip")
	FText FormatManaCostText(int32 ManaCost) const;

	// Format damage/healing text
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Tooltip")
	FText FormatDamageText(int32 Flat, float Coeff) const;

protected:
	// UI Components (bind these in Blueprint)
	UPROPERTY(meta = (BindWidget))
	UBorder* TooltipBorder;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* MainContent;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HeaderBox;

	UPROPERTY(meta = (BindWidget))
	UImage* SkillIcon;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* SkillNameText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* SkillSchoolText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* SkillLevelText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* SkillDescriptionText;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* StatsBox;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CooldownText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ManaCostText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DamageText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* RangeText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* EffectTypeText;

	// Separators
	UPROPERTY(meta = (BindWidget))
	UImage* Separator1;

	UPROPERTY(meta = (BindWidget))
	UImage* Separator2;

	// Current skill data
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tooltip")
	FPlayerSkillData CurrentSkillData;

	// Visual settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Tooltip")
	FVector2D TooltipOffset = FVector2D(20.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Tooltip")
	float FadeInDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Tooltip")
	float FadeOutDuration = 0.1f;

	// School colors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Tooltip")
	TMap<ESkillSchool, FLinearColor> SchoolColors;

	// Effect type colors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Tooltip")
	TMap<ESkillEffectType, FLinearColor> EffectTypeColors;

	// Default skill icon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Tooltip")
	UTexture2D* DefaultSkillIcon;

private:
	// Initialize colors
	void InitializeColors();

	// Check if tooltip is currently visible
	bool bIsVisible = false;

	/** Handle for async texture loading */
	TSharedPtr<FStreamableHandle> StreamableHandle;

	/** Sets the icon texture */
	void SetIconTexture(UTexture2D* Texture);

	/** Sets default/placeholder icon */
	void SetDefaultIcon();

	/** Helper function for async asset loading */
	void AsyncLoad(const FSoftObjectPath& Path, FSimpleDelegate Callback);

	/** Load skill icon */
	void LoadSkillIcon();
};