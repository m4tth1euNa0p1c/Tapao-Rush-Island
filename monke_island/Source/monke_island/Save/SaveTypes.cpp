// monke_island

#include "Save/SaveTypes.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"

DEFINE_LOG_CATEGORY(LogMonkeSave);

const TCHAR* FSaveMessages::TableId = TEXT("MonkeSave");

void FSaveMessages::EnsureRegistered()
{
	if (FStringTableRegistry::Get().FindStringTable(FName(TableId)).IsValid())
	{
		return;
	}

#define LOC_DEFINE_REGION
	LOCTABLE_NEW("MonkeSave", "MonkeSave");
	LOCTABLE_SETSTRING("MonkeSave", "Error_InvalidName", "Nom de campagne invalide : vide, trop long ou avec un caractère interdit.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_InvalidRequest", "Demande refusée : valeurs incohérentes.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_NoCampaignLoaded", "Aucune campagne chargée.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_CampaignNotFound", "Campagne introuvable. Choisissez-en une autre.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_Corrupted", "Sauvegarde endommagée et aucune copie valide. Les autres campagnes restent disponibles.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_FutureVersion", "Cette campagne vient d'une version plus récente du jeu. Mettez le jeu à jour.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_ReadFailed", "Lecture de la sauvegarde impossible. Réessayez.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_WriteFailed", "Échec de la sauvegarde. La dernière sauvegarde valide est conservée. Vérifiez l'espace disque.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_ValidationFailed", "Échec de la sauvegarde : vérification impossible. La dernière sauvegarde valide est conservée.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_PathRejected", "Emplacement de sauvegarde refusé.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_Timeout", "La sauvegarde prend trop de temps. Elle continue en arrière-plan.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_Cancelled", "Sauvegarde annulée après un échec précédent.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_ExpeditionAlreadyInProgress", "Une expédition est déjà en cours.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_ExpeditionMismatch", "Ce bilan ne correspond pas à l'expédition en cours.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_AlreadyApplied", "Ce bilan a déjà été appliqué.");
	LOCTABLE_SETSTRING("MonkeSave", "Error_InsufficientEquipment", "Équipement insuffisant pour partir.");
	LOCTABLE_SETSTRING("MonkeSave", "Info_LoadedFromBackup", "Le fichier principal était endommagé : la dernière copie valide a été chargée.");
	LOCTABLE_SETSTRING("MonkeSave", "Info_AbandonedExpedition", "Expédition interrompue : retour au galion. Équipement engagé perdu ({0}), aucun butin crédité.");
	LOCTABLE_SETSTRING("MonkeSave", "Indicator_Saving", "Sauvegarde…");
	LOCTABLE_SETSTRING("MonkeSave", "Indicator_Saved", "Sauvegardé");
	LOCTABLE_SETSTRING("MonkeSave", "Indicator_Failed", "Échec de la sauvegarde");
#undef LOC_DEFINE_REGION
}

FText FSaveMessages::ForError(ESaveError Error)
{
	EnsureRegistered();
	switch (Error)
	{
	case ESaveError::None:							return FText::GetEmpty();
	case ESaveError::InvalidName:					return LOCTABLE("MonkeSave", "Error_InvalidName");
	case ESaveError::InvalidRequest:				return LOCTABLE("MonkeSave", "Error_InvalidRequest");
	case ESaveError::NoCampaignLoaded:				return LOCTABLE("MonkeSave", "Error_NoCampaignLoaded");
	case ESaveError::CampaignNotFound:				return LOCTABLE("MonkeSave", "Error_CampaignNotFound");
	case ESaveError::Corrupted:						return LOCTABLE("MonkeSave", "Error_Corrupted");
	case ESaveError::FutureVersion:					return LOCTABLE("MonkeSave", "Error_FutureVersion");
	case ESaveError::ReadFailed:					return LOCTABLE("MonkeSave", "Error_ReadFailed");
	case ESaveError::WriteFailed:					return LOCTABLE("MonkeSave", "Error_WriteFailed");
	case ESaveError::ValidationFailed:				return LOCTABLE("MonkeSave", "Error_ValidationFailed");
	case ESaveError::PathRejected:					return LOCTABLE("MonkeSave", "Error_PathRejected");
	case ESaveError::Timeout:						return LOCTABLE("MonkeSave", "Error_Timeout");
	case ESaveError::Cancelled:						return LOCTABLE("MonkeSave", "Error_Cancelled");
	case ESaveError::ExpeditionAlreadyInProgress:	return LOCTABLE("MonkeSave", "Error_ExpeditionAlreadyInProgress");
	case ESaveError::ExpeditionMismatch:			return LOCTABLE("MonkeSave", "Error_ExpeditionMismatch");
	case ESaveError::AlreadyApplied:				return LOCTABLE("MonkeSave", "Error_AlreadyApplied");
	case ESaveError::InsufficientEquipment:			return LOCTABLE("MonkeSave", "Error_InsufficientEquipment");
	}
	return LOCTABLE("MonkeSave", "Error_ReadFailed");
}

FText FSaveMessages::LoadedFromBackup()
{
	EnsureRegistered();
	return LOCTABLE("MonkeSave", "Info_LoadedFromBackup");
}

FText FSaveMessages::AbandonedExpedition(int32 LostItemCount)
{
	EnsureRegistered();
	return FText::Format(LOCTABLE("MonkeSave", "Info_AbandonedExpedition"), FText::AsNumber(LostItemCount));
}

FText FSaveMessages::ForIndicator(ESaveIndicatorState State)
{
	EnsureRegistered();
	switch (State)
	{
	case ESaveIndicatorState::Saving:	return LOCTABLE("MonkeSave", "Indicator_Saving");
	case ESaveIndicatorState::Saved:	return LOCTABLE("MonkeSave", "Indicator_Saved");
	case ESaveIndicatorState::Failed:	return LOCTABLE("MonkeSave", "Indicator_Failed");
	case ESaveIndicatorState::Idle:		break;
	}
	return FText::GetEmpty();
}

FString LexToString(ESaveError Error)
{
	const UEnum* Enum = StaticEnum<ESaveError>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Error)) : FString::FromInt(static_cast<int32>(Error));
}

FString LexToString(ESaveOperation Operation)
{
	const UEnum* Enum = StaticEnum<ESaveOperation>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Operation)) : FString::FromInt(static_cast<int32>(Operation));
}
