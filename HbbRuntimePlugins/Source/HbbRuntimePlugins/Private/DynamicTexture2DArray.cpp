// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicTexture2DArray.h"
#include "DynamicTexture2DArrayComponent.h"
#include "RenderGraphBuilder.h"


UDynamicTexture2DArray::UDynamicTexture2DArray(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Comp = (UDynamicTexture2DArrayComponent*)GetOuter();
}

void UDynamicTexture2DArray::SetSourceTextures(TArray<TSoftObjectPtr<UTexture2D>>NewSourceTextures)
{
	SourceTextures.Empty();
	SourceTextures.Reserve(NewSourceTextures.Num());
	for(auto& i : NewSourceTextures)
	{
		UTexture2D* texObject = nullptr;
		if(!i || !i.IsValid())
		{
			texObject = i.LoadSynchronous();
		}
		else
		{
			texObject = i.Get();
		}
		SourceTextures.Add(texObject);
		if(texObject->NeverStream == 0 )
		{
			texObject->NeverStream = 1;
			texObject->UpdateResource();
		}
	}
	UpdateResource();
	UpdateFromSourceTextures(-1);
}

void UDynamicTexture2DArray::SetSourceTexture(TSoftObjectPtr<UTexture2D> NewSourceTexture, int32 index )
{
	if(NewSourceTexture)
	{
		if(SourceTextures.Num()>0 && SourceTextures.IsValidIndex(index) )
		{
			SourceTextures[index] = NewSourceTexture;
			UTexture2D* texObject = nullptr;
			if(!SourceTextures[index])
			{
				texObject = SourceTextures[index].LoadSynchronous();
			}
			else
			{
				texObject = SourceTextures[index].Get();
			}
			if(texObject->NeverStream == 0)
			{
				texObject->NeverStream = 1;
				texObject->UpdateResource();
			}
		}
	}
	UpdateResource();
	UpdateFromSourceTextures(index);
}

FTextureResource* UDynamicTexture2DArray::CreateResource()
{
	if(SourceTextures.Num()>0&&SourceTextures[0])
	{
		TargetTextureSize = SourceTextures[0]->GetSizeX();
		TargetPixelFormat = SourceTextures[0]->GetPixelFormat();
		if(GetResource() != nullptr)
		{
			auto currentRes = static_cast<FDynamicTexture2DArrayResource*>(GetResource());
			if(currentRes->GetSizeX() == TargetTextureSize && currentRes->GetPixelFormat() == TargetPixelFormat)
			{
				return GetResource();
			}
		}
		auto newResource = new FDynamicTexture2DArrayResource(this,TargetTextureSize,TargetPixelFormat,SourceTextures[0]->GetNumMips(),SourceTextures.Num(),SourceTextures[0]->SRGB);
		SetResource(newResource);
		return newResource;
	}
	else
	{
		if(GetResource() != nullptr)
		{
			return GetResource();
		}
		else
		{
			ReleaseResource();
			return nullptr;
		}
	}
}

void UDynamicTexture2DArray::UpdateResource()
{
	auto currentRes = static_cast<FDynamicTexture2DArrayResource*>(GetResource());
	if(GetResource() == nullptr
		|| (TargetPixelFormat != currentRes->GetPixelFormat()
		|| currentRes->GetSizeX() != TargetTextureSize)
		|| SourceTextures.Num() != currentRes->GetNumSlices()
		|| bForceUpdate)
	{
		bForceUpdate = false;
		UTexture::UpdateResource();
	}
}

void UDynamicTexture2DArray::ForceUpdateResource()
{
	bForceUpdate = true;
	UpdateResource();
}

void UDynamicTexture2DArray::UpdateFromSourceTextures(int32 index)
{
	ENQUEUE_RENDER_COMMAND(UpdateDynamicTexture2DArray)(
	[this,index](FRHICommandListImmediate& RHICmdList)
	{
		UpdateFromSourceTextures_RenderThread(RHICmdList,index);
	});
}

void UDynamicTexture2DArray::UpdateFromSourceTextures_RenderThread(FRHICommandListImmediate& RHICmdList, int32 index)
{
	check(IsInRenderingThread());
	
	if(this->GetResource() == nullptr
	|| this->GetResource()->TextureRHI == nullptr
	|| SourceTextures.Num() <= 0)
	{
		return;
	}
	auto resource = static_cast<FDynamicTexture2DArrayResource*>(this->GetResource());
	auto DestinationTextureArrayRHI = resource->TextureRHI;
	if(index == -1)
	{
		for (int32 ArrayIndex = 0; ArrayIndex < SourceTextures.Num(); ArrayIndex++)
		{
			for (uint32 MipIndex = 0; MipIndex < resource->GetNumMips(); MipIndex++)
			{	
				if(!SourceTextures[ArrayIndex].IsValid())
				{
					continue;
				}
				{
					UTexture2D* srcTex = SourceTextures[ArrayIndex].Get();
					auto SourceTextureRHI = srcTex->GetResource()->TextureRHI;
				
					FRHICopyTextureInfo CopyInfo={};
					CopyInfo.SourceMipIndex = MipIndex;
					//
					CopyInfo.DestMipIndex = MipIndex;
					CopyInfo.DestSliceIndex = ArrayIndex;
					//
					auto CopySize = SourceTextureRHI->GetMipDimensions(MipIndex);
					CopyInfo.Size = CopySize;
				
					TransitionAndCopyTexture(
						RHICmdList,
						SourceTextureRHI,
						DestinationTextureArrayRHI,
						CopyInfo);
				}
			}
		}
	}
	else if(SourceTextures.IsValidIndex(index))
	{
		for (uint32 MipIndex = 0; MipIndex < resource->GetNumMips(); MipIndex++)
		{	
			if(!SourceTextures[index].IsValid())
			{
				continue;
			}
			{
				UTexture2D* srcTex = SourceTextures[index].Get();
				auto SourceTextureRHI = srcTex->GetResource()->TextureRHI;
				
				FRHICopyTextureInfo CopyInfo={};
				CopyInfo.SourceMipIndex = MipIndex;
				//
				CopyInfo.DestMipIndex = MipIndex;
				CopyInfo.DestSliceIndex = index;
				//
				auto CopySize = SourceTextureRHI->GetMipDimensions(MipIndex);
				CopyInfo.Size = CopySize;
				
				TransitionAndCopyTexture(
					RHICmdList,
					SourceTextureRHI,
					DestinationTextureArrayRHI,
					CopyInfo);
			}
		}
	}
	// RHICmdList.BlockUntilGPUIdle();
	// RHICmdList.SubmitCommandsAndFlushGPU();
	// RHICmdList.FlushResources();
	
	// Async(EAsyncExecution::Thread, [this]()
	// {
	// 	// for(auto& i : SourceTextures)
	// 	// {
	// 	// 	if(i)
	// 	// 	{
	// 	// 		i->NeverStream = 0;
	// 	// 		i->UpdateResource();
	// 	// 	}
	// 	// }
	// 	SourceTextures.Empty();
	// });
}

FDynamicTexture2DArrayResource::FDynamicTexture2DArrayResource(UDynamicTexture2DArray* InOwner , uint32 InSizeXY, EPixelFormat InFormat, uint32 InNumMips,
	uint32 InNumSlices, bool InbSRGB)
{
	SizeXY = InSizeXY;
	Format = InFormat;
	NumMips = InNumMips;
	NumSlices = InNumSlices;
	Filter = ESamplerFilter::SF_Bilinear;
	SamplerAddress = ESamplerAddressMode::AM_Wrap;
	bSRGB = InbSRGB;
    Owner = InOwner;
}

//
void FDynamicTexture2DArrayResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	FTextureResource::InitRHI(RHICmdList);
	//
	//
	FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2DArray(TEXT("FDynamicTexture2DArrayResource"),SizeXY,SizeXY,NumSlices,Format)
	.SetNumMips(NumMips)
	.SetFlags((bSRGB ? ETextureCreateFlags::SRGB : ETextureCreateFlags::None)  | ETextureCreateFlags::OfflineProcessed);
	
	TextureRHI = RHICreateTexture(Desc);

	if (Owner)
	{
		RHIBindDebugLabelName(TextureRHI, *Owner->GetName());
		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);
	}
	
	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		Filter,
		SamplerAddress,
		SamplerAddress,
		SamplerAddress
	);
	SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
	DeferredPassSamplerStateRHI = SamplerStateRHI;
	//TextureSRV = RHICmdList.CreateShaderResourceView(TextureRHI, /*MipLevel = */0, static_cast<uint8>(NumMips), Format);
}

void FDynamicTexture2DArrayResource::ReleaseRHI()
{
	FTextureResource::ReleaseRHI();
	
}