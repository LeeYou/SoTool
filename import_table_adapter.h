#pragma once
#include <Dbghelp.h>
#pragma comment(lib,"Dbghelp.lib")
#include <helper/SAdapterBase.h>
struct ImportTableItemData
{	
	bool bGroup;//�Ƿ���һ������
	SStringT strIdx;//��������
	SStringT strName;//������
};
class CImportTableTreeViewAdapter :public STreeAdapterBase<ImportTableItemData>
{
public:	
	CImportTableTreeViewAdapter() {}
	~CImportTableTreeViewAdapter() {}
	
	virtual void getView(SOUI::HTREEITEM loc, SWindow * pItem, pugi::xml_node xmlTemplate)
	{
		ItemInfo & ii = m_tree.GetItemRef((HSTREEITEM)loc);
		int itemType = getViewType(loc);
		if (pItem->GetChildrenCount() == 0)
		{
			switch (itemType)
			{
			case 0:xmlTemplate = xmlTemplate.child(L"item_group");
				break;
			case 1:xmlTemplate = xmlTemplate.child(L"item_data");
				break;
			}
			pItem->InitFromXml(xmlTemplate);
		}
		if (itemType == 0)
		{			
			SToggle *pSwitch = pItem->FindChildByName2<SToggle>(L"tgl_switch");
			
			pSwitch->SetVisible(ii.data.bGroup);
			pSwitch->SetToggle(IsItemExpanded(loc));
			pItem->FindChildByName(L"txt_dll_name")->SetWindowText(ii.data.strName);
			pItem->GetEventSet()->subscribeEvent(EVT_CMD, Subscriber(&CImportTableTreeViewAdapter::OnGroupPanleClick, this));//OnGroupPanleClick
		}
		else {
			SWindow * pWnd = pItem->FindChildByName2<SWindow>(L"txt_red");
			if (pWnd) pWnd->SetWindowText(S_CW2T(ii.data.strName));			
			pItem->FindChildByName(L"txt_idx")->SetWindowTextW(ii.data.strIdx);
			pItem->FindChildByName(L"txt_fun_name")->SetWindowTextW(ii.data.strName);
		}
		SOUI::HTREEITEM hParent;
		hParent = GetParentItem(loc);
		int iDep = 0;
		while (hParent != ITEM_ROOT)
		{
			++iDep;
			hParent = GetParentItem(hParent);
		}
		SWindow *containerWnd = pItem->FindChildByName(L"container");
		if (containerWnd)
		{
			SStringW strPos;
			strPos.Format(L"%d,0,-0,-0", iDep *10);
			containerWnd->SetAttribute(L"pos", strPos);			
		}
	}

	bool OnItemPanleClick(EventArgs *pEvt)
	{
		SItemPanel *pItemPanel = sobj_cast<SItemPanel>(pEvt->sender);
		SASSERT(pItemPanel);
		pItemPanel->ModifyState(WndState_Check, 0);
		STreeView *pTreeView = (STreeView*)pItemPanel->GetContainer();
		SOUI::HTREEITEM loc = (SOUI::HTREEITEM)pItemPanel->GetItemIndex();
		if (pTreeView)
		{
			pTreeView->SetSel(loc, TRUE);
		}
		return true;
	}

	int Updata(LPBYTE lpBaseAddress)
	{
		m_tree.DeleteAllItems();
		notifyBranchChanged(ITEM_ROOT);
		int i, j;
		
		if (lpBaseAddress == NULL)
		{
			return -1;
		}
		PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpBaseAddress;
		PIMAGE_NT_HEADERS32 pNtHeaders = (PIMAGE_NT_HEADERS32)(lpBaseAddress + pDosHeader->e_lfanew);
		//����һ���ǲ���һ����Ч��PE�ļ�
		if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE || IMAGE_NT_SIGNATURE != pNtHeaders->Signature)
		{			
			return -1;
		}

		if (pNtHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
		{
			//������rva
			DWORD Rva_import_table = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
			if (Rva_import_table == 0)
			{
				return -1;
			}
			PIMAGE_IMPORT_DESCRIPTOR pImportTable = (PIMAGE_IMPORT_DESCRIPTOR)ImageRvaToVa(
				(PIMAGE_NT_HEADERS)pNtHeaders,
				lpBaseAddress,
				Rva_import_table,
				NULL
			);
			IMAGE_IMPORT_DESCRIPTOR null_iid;
			IMAGE_THUNK_DATA32 null_thunk;
			memset(&null_iid, 0, sizeof(null_iid));
			memset(&null_thunk, 0, sizeof(null_thunk));
			//ÿ��Ԫ�ش�����һ�������DLL��
			ImportTableItemData data;

			data.bGroup = false;			
			data.strName = L"32λPE";
			InsertItem(data);
			for (i = 0; memcmp(pImportTable + i, &null_iid, sizeof(null_iid)) != 0; i++)
			{
				//LPCSTR: ���� const char*
				LPCSTR szDllName = (LPCSTR)ImageRvaToVa(
					(PIMAGE_NT_HEADERS)pNtHeaders, lpBaseAddress,
					pImportTable[i].Name, //DLL���Ƶ�RVA
					NULL);
				data.bGroup = TRUE;
				data.strName = S_CA2T(szDllName);
				HSTREEITEM hDll = InsertItem(data);
				// 		SetItemExpanded(hRoot, FALSE);			
				//IMAGE_TRUNK_DATA ���飨IAT�������ַ��ǰ��
				PIMAGE_THUNK_DATA32 pThunk = (PIMAGE_THUNK_DATA32)ImageRvaToVa(
					(PIMAGE_NT_HEADERS)pNtHeaders, lpBaseAddress,
					pImportTable[i].OriginalFirstThunk,
					NULL);
				int iFunCount = 0;
				for (j = 0; memcmp(pThunk + j, &null_thunk, sizeof(null_thunk)) != 0; j++)
				{
					//����ͨ��RVA�����λ�жϺ����ĵ��뷽ʽ��
					//������λΪ1������ŵ��룬�������Ƶ���
					if (pThunk[j].u1.AddressOfData & IMAGE_ORDINAL_FLAG32)
					{
						data.bGroup = false;
						data.strIdx.Format(L"%ld", pThunk[j].u1.AddressOfData & 0xffff);
						data.strName = L"����ŵ���";
						InsertItem(data, hDll);
					}
					else
					{
						//�����Ƶ��룬�����ٴζ��򵽺�����ź�����
						//ע�����ַ����ֱ���ã���Ϊ��Ȼ��RVA��
						PIMAGE_IMPORT_BY_NAME pFuncName = (PIMAGE_IMPORT_BY_NAME)ImageRvaToVa(
							(PIMAGE_NT_HEADERS)pNtHeaders, lpBaseAddress,
							pThunk[j].u1.AddressOfData,
							NULL);
						data.bGroup = false;
						data.strIdx.Format(L"%ld", pFuncName->Hint);
						data.strName = S_CA2T((char*)pFuncName->Name);
						InsertItem(data, hDll);
					}
					iFunCount = j + 1;
				}
				SStringT strFunCount;
				strFunCount.Format(L"(%d)", iFunCount);
				m_tree.GetItemRef(hDll).data.strName += strFunCount;
			}
			notifyBranchChanged(ITvAdapter::ITEM_ROOT);
		}
		else if (pNtHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_IA64 ||
			pNtHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
		{
			//PIMAGE_NT_HEADERS64 pNtHeaders64 = (PIMAGE_NT_HEADERS64)(lpBaseAddress + pDosHeader->e_lfanew);
			//������rva
			DWORD Rva_import_table = ((PIMAGE_NT_HEADERS64)pNtHeaders)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

			if (Rva_import_table == 0)
			{
				return -1;
			}
			PIMAGE_IMPORT_DESCRIPTOR pImportTable = (PIMAGE_IMPORT_DESCRIPTOR)ImageRvaToVa(
				(PIMAGE_NT_HEADERS)pNtHeaders,
				lpBaseAddress,
				Rva_import_table,
				NULL
			);
			IMAGE_IMPORT_DESCRIPTOR null_iid;
			//64λ��32�Ĳ�֮ͬ��
			IMAGE_THUNK_DATA64 null_thunk;
			memset(&null_iid, 0, sizeof(null_iid));
			memset(&null_thunk, 0, sizeof(null_thunk));
			//ÿ��Ԫ�ش�����һ�������DLL��
			ImportTableItemData data;
			data.bGroup = false;
			data.strName = L"64λPE";
			InsertItem(data);
			for (i = 0; memcmp(pImportTable + i, &null_iid, sizeof(null_iid)) != 0; i++)
			{
				//LPCSTR: ���� const char*
				LPCSTR szDllName = (LPCSTR)ImageRvaToVa(
					(PIMAGE_NT_HEADERS)pNtHeaders, lpBaseAddress,
					pImportTable[i].Name, //DLL���Ƶ�RVA
					NULL);
				data.bGroup = TRUE;
				data.strName = S_CA2T(szDllName);
				HSTREEITEM hDll = InsertItem(data);
				// 		SetItemExpanded(hRoot, FALSE);			
				//IMAGE_TRUNK_DATA ���飨IAT�������ַ��ǰ��
				PIMAGE_THUNK_DATA64 pThunk = (PIMAGE_THUNK_DATA64)ImageRvaToVa(
					(PIMAGE_NT_HEADERS)pNtHeaders, lpBaseAddress,
					pImportTable[i].OriginalFirstThunk,
					NULL);
				int iFunCount = 0;
				for (j = 0; memcmp(pThunk + j, &null_thunk, sizeof(null_thunk)) != 0; j++)
				{
					//����ͨ��RVA�����λ�жϺ����ĵ��뷽ʽ��
					//������λΪ1������ŵ��룬�������Ƶ���
					if (pThunk[j].u1.AddressOfData & IMAGE_ORDINAL_FLAG64)
					{
						data.bGroup = false;
						data.strIdx.Format(L"%ld", IMAGE_ORDINAL64(pThunk[j].u1.AddressOfData));
						data.strName = L"����ŵ���";
						InsertItem(data, hDll);
					}
					else
					{
						//�����Ƶ��룬�����ٴζ��򵽺�����ź�����
						//ע�����ַ����ֱ���ã���Ϊ��Ȼ��RVA��
						PIMAGE_IMPORT_BY_NAME pFuncName = (PIMAGE_IMPORT_BY_NAME)ImageRvaToVa(
							(PIMAGE_NT_HEADERS)pNtHeaders, lpBaseAddress,
							pThunk[j].u1.AddressOfData,
							NULL);
						data.bGroup = false;
						data.strIdx.Format(L"%ld", pFuncName->Hint);
						data.strName = S_CA2T((char*)pFuncName->Name);
						InsertItem(data, hDll);
					}
					iFunCount = j + 1;
				}
				SStringT strFunCount;
				strFunCount.Format(L"(%d)", iFunCount);
				m_tree.GetItemRef(hDll).data.strName += strFunCount;
			}
			notifyBranchChanged(ITvAdapter::ITEM_ROOT);
		}		
		
		return 0;
	}
	
	void HandleTreeViewContextMenu(CPoint pt, SItemPanel * pItem, HWND hWnd)
	{
		if (pItem)
		{
			SOUI::HTREEITEM loc = (SOUI::HTREEITEM)pItem->GetItemIndex();
			ItemInfo & ii = m_tree.GetItemRef((HSTREEITEM)loc);
			if (ii.data.bGroup)
			{
				SMenuEx menu;
				menu.LoadMenu(_T("smenuex:menuex_group"));
				int iCmd = menu.TrackPopupMenu(TPM_RETURNCMD, pt.x, pt.y, hWnd);
				ImportTableItemData data;
				switch (iCmd)
				{
				case 1:  //��ӷ���
				{
					SOUI::HTREEITEM hParent = GetParentItem(loc);					
					data.bGroup = true;
					data.strName = L"��Ӳ�����";
					SOUI::HTREEITEM hItem=InsertItem(data,hParent,loc);
					notifyBranchChanged(hParent);
				}break;
				case 2:  //ɾ������
				{
					SOUI::HTREEITEM hParent = GetParentItem(loc);
					DeleteItem(loc);
					notifyBranchChanged(hParent);
				}
				break;
				case 3:  //���������һ������
				{									
					data.bGroup = false;
					data.strIdx = L"9527";
					data.strName = L"��Ӳ�������";
					SOUI::HTREEITEM hItem = InsertItem(data, loc);
					notifyBranchChanged(loc);
				}
				break;
				case 4:  //���������һ���ӷ���
				{
					data.bGroup = true;
					data.strIdx = L"9527";
					data.strName = L"��Ӳ�������";
					SOUI::HTREEITEM hItem = InsertItem(data, loc);
					notifyBranchChanged(loc);
				}
				break;
				}
			}
			else
			{
				SMenuEx menu;
				menu.LoadMenu(_T("smenuex:menuex_item"));
				int iCmd = menu.TrackPopupMenu(TPM_RETURNCMD, pt.x, pt.y, hWnd);
				ImportTableItemData data;
				switch (iCmd)
				{
				case 1:
				{					
					SOUI::HTREEITEM hParent = GetParentItem(loc);
					data.bGroup = false;
					data.strIdx = L"008";
					data.strName = L"��Ӳ�������";
					SOUI::HTREEITEM hItem = InsertItem(data, hParent, loc);
					notifyBranchChanged(hParent);
				}
				break;
				case 2:  //ɾ��
				{
					SOUI::HTREEITEM hParent = GetParentItem(loc);
					DeleteItem(loc);
					notifyBranchChanged(hParent);
				}break;
				}
			}
		}	
		else
		{
			SMenuEx menu;
			menu.LoadMenu(_T("smenuex:menuex_none"));
			int iCmd = menu.TrackPopupMenu(TPM_RETURNCMD, pt.x, pt.y, hWnd);
			switch (iCmd)
			{
			case 1:  //��ӷ���
			{
				ImportTableItemData data;
				data.bGroup = true;
				data.strName = L"�հ���Ӳ�����";
				SOUI::HTREEITEM hItem = InsertItem(data);
				notifyBranchChanged(ITEM_ROOT);
			}			
			}
		}
	}

	bool OnGroupPanleClick(EventArgs *pEvt)
	{
		SItemPanel *pItem = sobj_cast<SItemPanel>(pEvt->sender);
		SToggle *pSwitch = pItem->FindChildByName2<SToggle>(L"tgl_switch");

		SOUI::HTREEITEM loc = (SOUI::HTREEITEM)pItem->GetItemIndex();
		ExpandItem(loc, ITvAdapter::TVC_TOGGLE);
		pSwitch->SetToggle(IsItemExpanded(loc));
		return true;
	}

	bool OnSwitchClick(EventArgs *pEvt)
	{
		SToggle *pToggle = sobj_cast<SToggle>(pEvt->sender);
		SASSERT(pToggle);
		SItemPanel *pItem = sobj_cast<SItemPanel>(pToggle->GetRoot());
		SASSERT(pItem);
		SOUI::HTREEITEM loc = (SOUI::HTREEITEM)pItem->GetItemIndex();
		ExpandItem(loc, ITvAdapter::TVC_TOGGLE);
		return true;
	}

	virtual int getViewType(SOUI::HTREEITEM hItem) const
	{
		ItemInfo & ii = m_tree.GetItemRef((HSTREEITEM)hItem);
		if (ii.data.bGroup) return 0;
		else return 1;
	}
	
	virtual int getViewTypeCount() const
	{
		return 2;
	}
	
};