class OVT_BaseUpgradeTownPatrol : OVT_BasePatrolUpgrade
{
	[Attribute("1500", desc: "Max range of towns to patrol")]
	float m_fRange;
	
	protected OVT_TownManagerComponent m_Towns;
	protected ref array<ref OVT_TownData> m_TownsInRange;
	protected ref map<ref int, ref EntityID> m_Patrols;
	protected ref map<int, bool> m_SpottedPatrols;
		
	override void PostInit()
	{
		super.PostInit();
		
		m_Towns = OVT_Global.GetTowns();
		m_TownsInRange = new array<ref OVT_TownData>;
		m_Patrols = new map<ref int, ref EntityID>;
		m_SpottedPatrols = new map<int, bool>;
		
		OVT_Global.GetTowns().GetTownsWithinDistance(m_BaseController.GetOwner().GetOrigin(), m_fRange, m_TownsInRange);
	}
	
	override void OnUpdate(int timeSlice)
	{
		OVT_TownModifierSystem system = m_Towns.GetModifierSystem(OVT_TownStabilityModifierSystem);
		if(!system) return;
		
		OVT_TownModifierSystem support = m_Towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if(!support) return;
		
		//Check on our patrols and update stability/support
		for(int i=0; i< m_Patrols.Count(); i++)
		{
			int townId = m_Patrols.GetKey(i);
			EntityID id = m_Patrols.GetElement(i);
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			if(ent)
			{
				SCR_AIGroup group = SCR_AIGroup.Cast(ent);
				if(group.GetAgentsCount() > 0)
				{
					OVT_TownData town = m_Towns.m_Towns[townId];
					float dist = vector.Distance(town.location, group.GetOrigin());
					if(town.SupportPercentage() > 25 && !m_SpottedPatrols[townId] && dist < m_Towns.GetTownRange(town))
					{
						m_SpottedPatrols[townId] = true;
						OVT_Global.GetNotify().SendTextNotification("PatrolSpotted",-1,m_Towns.GetTownName(townId));
					}
					if(dist < 50)
					{						
						if(town.support >= 75)
						{
							system.TryAddByName(townId, "RecentPatrolNegative");
							system.RemoveByName(townId, "RecentPatrolPositive");
						}else{
							system.TryAddByName(townId, "RecentPatrolPositive");
							system.RemoveByName(townId, "RecentPatrolNegative");
						}			
						support.TryAddByName(townId, "RecentPatrol");			
					}
				}
			}
		}
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		foreach(OVT_TownData town : m_TownsInRange)
		{		
			int townID = m_Towns.GetTownID(town);	

			if(!m_Patrols.Contains(townID))
			{
				if(OVT_Global.PlayerInRange(town.location, 5000)){
					int newres = BuyTownPatrol(town, threat);
					spent += newres;
					resources -= newres;
					if(resources<0){resources=0}
				}
			}
			if(resources <= 0) break;
			if(m_Patrols.Contains(townID)){
				//Check if theyre back
				SCR_AIGroup aigroup = GetGroup(m_Patrols[townID]);
				if(!aigroup) {
					m_Patrols.Remove(townID);
					continue;
				}
				float distance = vector.Distance(aigroup.GetOrigin(), m_BaseController.GetOwner().GetOrigin());
				int agentCount = aigroup.GetAgentsCount();
				if(distance < 20 || agentCount == 0)
				{
					//Recover any resources
					m_occupyingFactionManager.RecoverResources(agentCount * OVT_Global.GetConfig().m_Difficulty.baseResourceCost);
					
					m_Patrols.Remove(townID);
					SCR_EntityHelper.DeleteEntityAndChildren(aigroup);	
					
					//send another one
					if(OVT_Global.PlayerInRange(town.location, 5000)){
						int newres = BuyTownPatrol(town, threat);
					
						spent += newres;
						resources -= newres;	
					}		
				}
			}
		}
		
		return spent;
	}
	
	protected int BuyTownPatrol(OVT_TownData town, float threat)
	{
		int townID = OVT_Global.GetTowns().GetTownID(town);
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
				
		ResourceName res = faction.m_aLightTownPatrolPrefab;
		if(threat > 15) res = faction.GetRandomGroupByType(OVT_GroupType.LIGHT_INFANTRY);
		if(threat > 50) res = faction.GetRandomGroupByType(OVT_GroupType.HEAVY_INFANTRY);		
		
		BaseWorld world = GetGame().GetWorld();
		
		vector pos = m_BaseController.GetOwner().GetOrigin();
		pos = OVT_Global.GetRandomNonOceanPositionNear(pos, 15);
		
		float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
		if (pos[1] < surfaceY)
		{
			pos[1] = surfaceY;
		}
		
		IEntity group = OVT_Global.SpawnEntityPrefab(res, pos);
		
		m_Groups.Insert(group.GetID());
		m_Patrols[townID] = group.GetID();
		m_SpottedPatrols[townID] = false;
		
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
		
		AddWaypoints(aigroup, town);
		
		int newres = aigroup.m_aUnitPrefabSlots.Count() * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
			
		return newres;
	}
	
	protected void AddWaypoints(SCR_AIGroup aigroup, OVT_TownData town)
	{		
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		array<RplId> shops = m_Economy.GetAllShopsInTown(town);
		if(shops.Count() == 0)
		{
			//To-Do: find some random buildings
		}
							
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnPatrolWaypoint(town.location));			
				
		vector pos = OVT_Global.GetRandomNonOceanPositionNear(town.location, 250);
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(pos));			
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnWaitWaypoint(pos, s_AIRandomGenerator.RandFloatXY(15, 50)));								
		
		pos = OVT_Global.GetRandomNonOceanPositionNear(town.location, 250);
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(pos));			
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnWaitWaypoint(pos, s_AIRandomGenerator.RandFloatXY(15, 50)));								
		
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnPatrolWaypoint(m_BaseController.GetOwner().GetOrigin()));		
	}
	
	override OVT_BaseUpgradeData Serialize()
	{
		return null;
	}
	
}