#pragma once
#include "game.h"
#include "replication.h"
#include "ue4.h"

namespace Hooks
{
    uint64 GetNetMode(UWorld* World)
    {
        return 2; //ENetMode::NM_ListenServer;
    }

    void TickFlush(UNetDriver* NetDriver, float DeltaSeconds)
    {
        if (!NetDriver)
            return;

        if (NetDriver->IsA(UIpNetDriver::StaticClass()) && NetDriver->ClientConnections.Num() > 0 && NetDriver->ClientConnections[0]->InternalAck == false)
        {
            Replication::ServerReplicateActors(NetDriver, DeltaSeconds);
        }

        Functions::NetDriver::TickFlush(NetDriver, DeltaSeconds);
    }

    void WelcomePlayer(UWorld* World, UNetConnection* IncomingConnection)
    {
        Functions::World::WelcomePlayer(GetWorld(), IncomingConnection);
    }

    char KickPlayer(__int64 a1, __int64 a2, __int64 a3)
    {
        return 0;
    }

    void World_NotifyControlMessage(UWorld* World, UNetConnection* Connection, uint8 MessageType, void* Bunch)
    {
        Functions::World::NotifyControlMessage(GetWorld(), Connection, MessageType, Bunch);
    }

    APlayerController* SpawnPlayActor(UWorld* World, UPlayer* NewPlayer, ENetRole RemoteRole, FURL& URL, void* UniqueId, SDK::FString& Error, uint8 NetPlayerIndex)
    {
        auto PlayerController = (AFortPlayerControllerAthena*)Functions::World::SpawnPlayActor(GetWorld(), NewPlayer, RemoteRole, URL, UniqueId, Error, NetPlayerIndex);
        NewPlayer->PlayerController = PlayerController;
        PlayerController->SetOwner((AActor*)NewPlayer);
        PlayerController->OnRep_Owner();
		
        AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;
        PlayerState->SetOwner(PlayerController);
        PlayerState->OnRep_Owner();
		
        auto Pawn = SpawnActor<APlayerPawn_Athena_C>(GetPlayerController()->Pawn->K2_GetActorLocation(), PlayerController);

        Pawn->bCanBeDamaged = false;

        PlayerController->Pawn = Pawn;
        Pawn->SetOwner(PlayerController);
        Pawn->Owner = PlayerController;
        Pawn->OnRep_Owner();
        PlayerController->OnRep_Pawn();
        PlayerController->Possess(Pawn);

        PlayerController->bHasClientFinishedLoading = true;
        PlayerController->bHasServerFinishedLoading = true;
        PlayerController->OnRep_bHasServerFinishedLoading();

        PlayerState->bHasFinishedLoading = true;
        PlayerState->bHasStartedPlaying = true;
        PlayerState->OnRep_bHasStartedPlaying();

        PlayerState->CharacterBodyType = EFortCustomBodyType::LargeAndMedium;
        PlayerState->LocalCharacterBodyType = EFortCustomBodyType::LargeAndMedium;
        PlayerState->OnRep_CharacterBodyType();
        PlayerState->CharacterGender = EFortCustomGender::Female;
        PlayerState->LocalCharacterGender = EFortCustomGender::Female;
        PlayerState->OnRep_CharacterGender();
				
        PlayerState->LocalCharacterParts[0] = UObject::FindObject<UCustomCharacterPart>("F_Med_Head1.F_Med_Head1");
        PlayerState->LocalCharacterParts[1] = UObject::FindObject<UCustomCharacterPart>("F_Med_Soldier_01.F_Med_Soldier_01");
        
        PlayerState->CharacterParts[0] = UObject::FindObject<UCustomCharacterPart>("F_Med_Head1.F_Med_Head1");
        PlayerState->CharacterParts[1] = UObject::FindObject<UCustomCharacterPart>("F_Med_Soldier_01.F_Med_Soldier_01");
        
        Functions::PlayerState::OnRep_CharacterParts(PlayerState);

        //Pawn->PlayerState = PlayerState;
        //Pawn->OnRep_PlayerState();
        //Functions::PlayerState::OnRep_CharacterParts((AFortPlayerState*)Pawn->PlayerState);

        PlayerState->OnRep_HeroType();

        auto QuickBars = SpawnActor<AFortQuickBars>({ -280, 400, 3000 }, PlayerController);
        PlayerController->QuickBars = QuickBars;
        PlayerController->OnRep_QuickBar();

        static auto Def = UObject::FindObject<UFortWeaponItemDefinition>("WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01");
        auto TempItemInstance = Def->CreateTemporaryItemInstanceBP(1, 1);
        TempItemInstance->SetOwningControllerForTemporaryItem(PlayerController);

        ((UFortWorldItem*)TempItemInstance)->ItemEntry.Count = 1;

        auto ItemEntry = ((UFortWorldItem*)TempItemInstance)->ItemEntry;
        PlayerController->WorldInventory->Inventory.ReplicatedEntries.Add(ItemEntry);
        PlayerController->QuickBars->ServerAddItemInternal(ItemEntry.ItemGuid, EFortQuickBars::Primary, 0);

        PlayerController->WorldInventory->HandleInventoryLocalUpdate();
        PlayerController->HandleWorldInventoryLocalUpdate();
        PlayerController->OnRep_QuickBar();
        PlayerController->QuickBars->OnRep_PrimaryQuickBar();
        PlayerController->QuickBars->OnRep_SecondaryQuickBar();

        if (PlayerController->Pawn)
        {
            if (PlayerController->Pawn->PlayerState)
            {
                PlayerState->TeamIndex = EFortTeam::HumanPvP_Team2;
                PlayerState->OnRep_PlayerTeam();
                PlayerState->SquadId = 1;
                PlayerState->OnRep_SquadId();
            }
        }

        return PlayerController;
    }

    bool DestroySwappedPC(UWorld* World, UNetConnection* Connection)
    {
        return Functions::World::DestroySwappedPC(GetWorld(), Connection);
    }

    void Beacon_NotifyControlMessage(AOnlineBeaconHost* Beacon, UNetConnection* Connection, uint8 MessageType, void* Bunch)
    {
        printf("Recieved control message %i\n", MessageType);

        if (MessageType == 15)
            return; // PCSwap no thx

        if (MessageType == 5) // PreLogin isnt really needed so we can just use this
        {
            auto _Bunch = reinterpret_cast<int64*>(Bunch);
            _Bunch[7] += (16 * 1024 * 1024);

            FString OnlinePlatformName = FString(L"");

            Functions::NetConnection::ReceiveFString(Bunch, Connection->ClientResponse);
            Functions::NetConnection::ReceiveFString(Bunch, Connection->RequestURL);
            Functions::NetConnection::ReceiveUniqueIdRepl(Bunch, Connection->PlayerID);
            Functions::NetConnection::ReceiveFString(Bunch, OnlinePlatformName);

            _Bunch[7] -= (16 * 1024 * 1024);

            Functions::World::WelcomePlayer(GetWorld(), Connection);
            return;
        }

        Functions::World::NotifyControlMessage(GetWorld(), Connection, MessageType, Bunch);
    }

    uint8 Beacon_NotifyAcceptingConnection(AOnlineBeacon* Beacon)
    {
        return Functions::World::NotifyAcceptingConnection(GetWorld());
    }

    void* SeamlessTravelHandlerForWorld(UEngine* Engine, UWorld* World)
    {
        return Functions::Engine::SeamlessTravelHandlerForWorld(Engine, GetWorld());
    }

    void* NetDebug(UObject* _this)
    {
        return nullptr;
    }

    void Listen()
    {
        printf("[UWorld::Listen]\n");

        AFortOnlineBeaconHost* HostBeacon = SpawnActor<AFortOnlineBeaconHost>();
        HostBeacon->ListenPort = 7777;
        Functions::OnlineBeaconHost::InitHost(HostBeacon);

        HostBeacon->NetDriverName = FName(282); // REGISTER_NAME(282,GameNetDriver)
        HostBeacon->NetDriver->NetDriverName = FName(282); // REGISTER_NAME(282,GameNetDriver)
        HostBeacon->NetDriver->World = GetWorld();

        GetWorld()->NetDriver = HostBeacon->NetDriver;
        GetWorld()->LevelCollections[0].NetDriver = HostBeacon->NetDriver;
        GetWorld()->LevelCollections[1].NetDriver = HostBeacon->NetDriver;

        Functions::OnlineBeacon::PauseBeaconRequests(HostBeacon, false);

        DETOUR_START
        DetourAttachE(Functions::World::WelcomePlayer, WelcomePlayer);
        DetourAttachE(Functions::Actor::GetNetMode, Hooks::GetNetMode);
        DetourAttachE(Functions::World::NotifyControlMessage, World_NotifyControlMessage);
        DetourAttachE(Functions::World::SpawnPlayActor, SpawnPlayActor);
        DetourAttachE(Functions::OnlineBeaconHost::NotifyControlMessage, Beacon_NotifyControlMessage);
        DetourAttachE(Functions::OnlineSession::KickPlayer, Hooks::KickPlayer);
        DETOUR_END
			
        return;
    }

    void ProcessEvent(UObject* Object, UFunction* Function, void* Parameters)
    {
        auto ObjectName = Object->GetFullName();
        auto FunctionName = Function->GetFullName();

		if (!bPlayButton && FunctionName.find("BP_PlayButton") != -1)
        {
            bPlayButton = true;
            Game::Start();
            printf("[Game::Start] Done\n");
        }

        if (bTraveled && FunctionName.find("ReadyToStartMatch") != -1)
        {
            EXECUTE_ONE_TIME
            {
                Game::OnReadyToStartMatch();
                Listen();
            }
        }

        /* // Crashes

        if (FunctionName.find("ServerReturnToMainMenu") != - 1)
        {
            GetPlayerController()->SwitchLevel(L"Frontend");
			
            bPlayButton = false;
            bTraveled = false;
        }

		*/

        return PEOriginal(Object, Function, Parameters);
    }
}
