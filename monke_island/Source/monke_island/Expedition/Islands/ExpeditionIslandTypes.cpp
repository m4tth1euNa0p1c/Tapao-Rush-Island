// monke_island

#include "Expedition/Islands/ExpeditionIslandTypes.h"

DEFINE_LOG_CATEGORY(LogMonkeIslands);

#define LOCTEXT_NAMESPACE "MonkeFragments"

FText ExpeditionIslands::GetErrorText(EFragmentError Error)
{
	switch (Error)
	{
	case EFragmentError::None:
		return FText::GetEmpty();
	case EFragmentError::AssetManagerUnavailable:
	case EFragmentError::CatalogNotFound:
	case EFragmentError::CatalogNotLoaded:
		return LOCTEXT("CatalogUnavailable", "Le catalogue des îles est indisponible. Réessayez dans un instant.");
	case EFragmentError::CatalogEmpty:
	case EFragmentError::NoValidFragment:
		return LOCTEXT("CatalogEmpty", "Aucune île n'est proposée pour le moment.");
	case EFragmentError::FragmentNotFound:
	case EFragmentError::NotInCatalog:
	case EFragmentError::InvalidFragmentId:
		return LOCTEXT("FragmentUnknown", "Cette île n'est pas disponible. Choisissez-en une autre.");
	case EFragmentError::MapMissing:
	case EFragmentError::InvalidZoneCount:
	case EFragmentError::InvalidAnchorages:
		return LOCTEXT("FragmentInvalid", "Cette île est incomplète et ne peut pas être jouée. Choisissez-en une autre.");
	case EFragmentError::NoSelection:
		return LOCTEXT("NoSelection", "Choisissez une île avant de partir.");
	case EFragmentError::NoAuthority:
		return LOCTEXT("NoAuthority", "Seul l'hôte choisit l'île de l'expédition.");
	case EFragmentError::LoadTimedOut:
		return LOCTEXT("LoadTimedOut", "Le chargement de l'île a pris trop de temps. Réessayez.");
	case EFragmentError::LoadCancelled:
		return LOCTEXT("LoadCancelled", "Le chargement de l'île a été annulé.");
	}
	return LOCTEXT("Unknown", "Erreur inconnue sur l'île choisie.");
}

const TCHAR* ExpeditionIslands::LexErrorName(EFragmentError Error)
{
	switch (Error)
	{
	case EFragmentError::None:						return TEXT("None");
	case EFragmentError::AssetManagerUnavailable:	return TEXT("AssetManagerUnavailable");
	case EFragmentError::CatalogNotFound:			return TEXT("CatalogNotFound");
	case EFragmentError::CatalogEmpty:				return TEXT("CatalogEmpty");
	case EFragmentError::NoValidFragment:			return TEXT("NoValidFragment");
	case EFragmentError::CatalogNotLoaded:			return TEXT("CatalogNotLoaded");
	case EFragmentError::FragmentNotFound:			return TEXT("FragmentNotFound");
	case EFragmentError::InvalidFragmentId:			return TEXT("InvalidFragmentId");
	case EFragmentError::MapMissing:				return TEXT("MapMissing");
	case EFragmentError::InvalidZoneCount:			return TEXT("InvalidZoneCount");
	case EFragmentError::InvalidAnchorages:			return TEXT("InvalidAnchorages");
	case EFragmentError::NotInCatalog:				return TEXT("NotInCatalog");
	case EFragmentError::NoSelection:				return TEXT("NoSelection");
	case EFragmentError::NoAuthority:				return TEXT("NoAuthority");
	case EFragmentError::LoadTimedOut:				return TEXT("LoadTimedOut");
	case EFragmentError::LoadCancelled:				return TEXT("LoadCancelled");
	}
	return TEXT("Unknown");
}

#undef LOCTEXT_NAMESPACE
