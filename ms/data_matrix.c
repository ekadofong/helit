// Copyright 2013 Tom SF Haines

// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

//   http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.



#include "data_matrix.h"

#include <stdlib.h>
#include <string.h>



float ToFloat_zero(void * data)
{
 return 0.0; 
}

float ToFloat_char(void * data)
{
 return *(char*)data; 
}

float ToFloat_short(void * data)
{
 return *(short*)data; 
}

float ToFloat_int(void * data)
{
 return *(int*)data; 
}

float ToFloat_long_long(void * data)
{
 return *(long long*)data; 
}

float ToFloat_unsigned_char(void * data)
{
 return *(unsigned char*)data; 
}

float ToFloat_unsigned_short(void * data)
{
 return *(unsigned short*)data; 
}

float ToFloat_unsigned_int(void * data)
{
 return *(unsigned int*)data; 
}

float ToFloat_unsigned_long_long(void * data)
{
 return *(unsigned long long*)data; 
}

float ToFloat_float(void * data)
{
 return *(float*)data; 
}

float ToFloat_double(void * data)
{
 return *(double*)data; 
}

float ToFloat_long_double(void * data)
{
 return *(long double*)data; 
}


ToFloat KindToFunc(const PyArray_Descr * descr)
{
 switch (descr->kind)
 {
  case 'b': // Boolean
  return ToFloat_char;
     
  case 'i': // Signed integer
   switch (descr->elsize)
   {
    case sizeof(char):      return ToFloat_char;
    case sizeof(short):     return ToFloat_short;
    case sizeof(int):       return ToFloat_int;
    case sizeof(long long): return ToFloat_long_long;
   }
  break;
     
  case 'u': // Unsigned integer
   switch (descr->elsize)
   {
    case sizeof(unsigned char):      return ToFloat_unsigned_char;
    case sizeof(unsigned short):     return ToFloat_unsigned_short;
    case sizeof(unsigned int):       return ToFloat_unsigned_int;
    case sizeof(unsigned long long): return ToFloat_unsigned_long_long;
   }
  break;
     
  case 'f': // Floating point
   switch (descr->elsize)
   {
    case sizeof(float):       return ToFloat_float;
    case sizeof(double):      return ToFloat_double;
    case sizeof(long double): return ToFloat_long_double;
   }
  break;
 }
  
 return ToFloat_zero;
}



void DataMatrix_init(DataMatrix * dm)
{
 // Not 100% sure this is needed, as this module is used by a module that will inevitably call it, but just incase and to make the compiler shut up about me not calling it...
  import_array1();
  
 // Initalise everything to nulls etc...
  dm->array = NULL;
  dm->dt = NULL;
  dm->weight_index = -1;
  dm->weight_scale = 1.0;
  dm->weight_cum = NULL;
  dm->exemplars = 0;
  dm->feats = 0;
  dm->dual_feats = 0;
  dm->mult = NULL;
  dm->fv = NULL;
  dm->feat_dims = 0;
  dm->feat_indices = NULL;
  dm->to_float = ToFloat_zero;
  dm->feats_conv = 0;
  dm->fv_conv = NULL;
  dm->ops_conv = 0;
  dm->conv = NULL;
}


void DataMatrix_deinit(DataMatrix * dm)
{
 Py_XDECREF(dm->array);
 dm->array = NULL;
 
 free(dm->dt);
 dm->dt = NULL;
 
 free(dm->weight_cum);
 dm->weight_cum = NULL;
 
 dm->exemplars = 0;
 dm->feats = 0;
 dm->dual_feats = 0;
 
 free(dm->mult);
 dm->mult = NULL;
 
 free(dm->fv);
 dm->fv = NULL;
 
 dm->feat_dims = 0;
 free(dm->feat_indices);
 dm->feat_indices = NULL;

 dm->to_float = ToFloat_zero;
 
 dm->feats_conv = 0;
 free(dm->fv_conv);
 dm->fv_conv = NULL;
 dm->ops_conv = 0;
 free(dm->conv);
 dm->conv = NULL;
}



void DataMatrix_set(DataMatrix * dm, PyArrayObject * array, DimType * dt, int weight_index, const char * conv_str)
{
 int i;
 
 // Clean up any previous state...
  DataMatrix_deinit(dm);
  if (array==NULL) return;
  
 // Store the data matrix and its dimension types...
  dm->array = array;
  Py_INCREF(dm->array);
  
  dm->dt = (DimType*)malloc(PyArray_NDIM(dm->array) * sizeof(DimType));
  for (i=0; i<PyArray_NDIM(dm->array); i++) dm->dt[i] = dt[i];
  
  dm->weight_index = weight_index;
  
 // Calculate the number of exemplars and features...
  dm->exemplars = 1;
  dm->feats = 1;
  
  for (i=0; i<PyArray_NDIM(dm->array); i++)
  {
   switch(dm->dt[i])
   {
    case DIM_DATA:
     dm->exemplars *= PyArray_DIMS(dm->array)[i];
    break;
     
    case DIM_DUAL:
     dm->exemplars *= PyArray_DIMS(dm->array)[i];
     dm->dual_feats += 1;
    break;
     
    case DIM_FEATURE:
     dm->feats *= PyArray_DIMS(dm->array)[i];
     dm->feat_dims += 1;
    break;
   }
  }
  
  dm->feats += dm->dual_feats;
  
  if ((dm->weight_index>=0)&&(dm->weight_index<dm->feats))
  {
   dm->feats -= 1; 
  }
  else dm->weight_index = -1;
  
 // Create the feature vector internal storage...  
  dm->fv = (float*)malloc(dm->feats * sizeof(float));
  
 // Create the index of all feature indices...
  dm->feat_indices = (int*)malloc(dm->feat_dims * sizeof(int));
  int os = 0;
  for (i=0; i<PyArray_NDIM(dm->array); i++)
  {
   if (dm->dt[i]==DIM_FEATURE)
   {
    dm->feat_indices[os] = i;
    os += 1;
   }
  }
  
 // Clean up any cache of cumulative weight...
  free(dm->weight_cum);
  dm->weight_cum = NULL;
  
 // Store a function pointer in the to_float variable that matches this array type...
  dm->to_float = KindToFunc(PyArray_DESCR(dm->array));
  
 // Prepare the conversion system...
  if (conv_str!=NULL)
  {
   dm->ops_conv = strlen(conv_str);
   dm->conv = (ConvertOp*)malloc(dm->ops_conv * sizeof(ConvertOp));
   
   int offset_external = 0;
   int offset_internal = 0;
   for (i=0; i<dm->ops_conv; i++)
   {
    // Find the index of the relevant convertor...
     int j=0;
     while (ListConvert[j]->code!=conv_str[i]) ++j;
    
    // Store the convertor...
     dm->conv[i].conv = ListConvert[j];
     dm->conv[i].offset_external = offset_external;
     dm->conv[i].offset_internal = offset_internal;
     
    // Offset accordingly...
     offset_external += dm->conv[i].conv->dim_ext;
     offset_internal += dm->conv[i].conv->dim_int;
   }
   
   dm->feats_conv = offset_internal;
   dm->fv_conv = (float*)malloc(dm->feats_conv * sizeof(float));
  }
  else
  {
   dm->feats_conv = dm->feats;
  }
  
 // Prepare the scale array...
  dm->mult = (float*)malloc(dm->feats_conv * sizeof(float));
  for (i=0; i<dm->feats_conv; i++) dm->mult[i] = 1.0;
}



int DataMatrix_exemplars(DataMatrix * dm)
{
 return dm->exemplars;  
}


int DataMatrix_features(DataMatrix * dm)
{
 return dm->feats_conv; 
}

int DataMatrix_ext_features(DataMatrix * dm)
{
 return dm->feats; 
}



void DataMatrix_set_scale(DataMatrix * dm, float * scale, float weight_scale)
{
 int i;
 for (i=0; i<dm->feats_conv; i++) dm->mult[i] = scale[i];
 dm->weight_scale = weight_scale;
}



float * DataMatrix_fv(DataMatrix * dm, int index, float * weight)
{
 float * ret = DataMatrix_ext_fv(dm, index, weight);
 ret = DataMatrix_to_int(dm, ret, dm->fv_conv); 
 return ret;
}


float * DataMatrix_ext_fv(DataMatrix * dm, int index, float * weight)
{
 int i;
 char * base = PyArray_DATA(dm->array);
 int next_dual_feat = dm->dual_feats-1;
   
 if (weight!=NULL) *weight = dm->weight_scale;
 
 // Indexing pass - go backwards through the dimensions, updating the index as we go and figuring out the base index for the output; do the dual dimensions as we go...
  for (i=PyArray_NDIM(dm->array)-1; i>=0; i--)
  {
   npy_intp size = PyArray_DIMS(dm->array)[i];
   npy_intp step = index % size;
   
   if (dm->dt[i]==DIM_DUAL)
   {
    if (dm->weight_index==next_dual_feat)
    {
     if (weight!=NULL) *weight *= step;
    }
    else
    {
     dm->fv[next_dual_feat] = step;
    }
    next_dual_feat -= 1;
   }
   
   if (dm->dt[i]!=DIM_FEATURE)
   {
    base += PyArray_STRIDES(dm->array)[i] * step;
    index /= size;
   }
  }
  
 // If the weight index is for a dual feature handle it we need to offset the positions...
  if ((dm->weight_index>=0)&&(dm->weight_index<dm->dual_feats))
  {
   for (i=dm->weight_index; i<dm->dual_feats-1; i++)
   {
    dm->fv[i] = dm->fv[i+1];
   }
  }
 
 // Second pass - iterate and do each feature in turn...
  int count = dm->feats - dm->dual_feats;
  int oi = dm->dual_feats;
  if (dm->weight_index>=0)
  {
   count += 1;
   if (dm->weight_index<dm->dual_feats) oi -= 1;
  }
  
  for (i=0; i<count; i++)
  {
   // Calculate the offset of this data item...
    char * pos = base;
    
    int j;
    int fi = i;
    for (j=dm->feat_dims-1; j>=0; j--)
    {
     int di = dm->feat_indices[j];
     npy_intp size = PyArray_DIMS(dm->array)[di];
     pos += PyArray_STRIDES(dm->array)[di] * (fi % size);
     fi /= size;
    }
   
   // Translate the various possibilities - the use of a function pointer makes this relativly efficient...
    if (i!=(dm->weight_index-dm->dual_feats))
    {
     dm->fv[oi] = dm->to_float(pos);
     oi += 1;
    }
    else
    {
     if (weight!=NULL) *weight *= dm->to_float(pos);
    }
  }

 // Return the internal storage we have written the information into...
  return dm->fv;
}



int DataMatrix_draw(DataMatrix * dm, PhiloxRNG * rng)
{
 // Two scenarios - all samples have the same weight, or they don't, resulting in different approaches...
  if (dm->weight_index<0) // All samples have the same weight
  { 
   // Uniform draw multiplied by the number of exemplars...
    float pos = dm->exemplars * PhiloxRNG_uniform(rng);
    
   // Return the index of the sample that is that far in...
    return (int)pos;
  }
  else // Samples have variable weights
  {
   // Build the array of cumulative weights if it has not already been cached...
    if (dm->weight_cum==NULL)
    {
     dm->weight_cum = (float*)malloc(dm->exemplars * sizeof(float));
     
     float sum = 0.0;
     int i;
     for (i=0; i<dm->exemplars; i++)
     {
      float weight;
      DataMatrix_fv(dm, i, &weight); // Inefficient - could write code to get just the weight out, but its as painful as the called function. May do later.
      sum += weight;
      dm->weight_cum[i] = sum;
     }
    }
    
   // Uniform draw multiplied by the total weight, as given by weight_cum...
    float pos = dm->weight_cum[dm->exemplars-1] * PhiloxRNG_uniform(rng);
    
   // Use a biased binary search (under the assumption that similar weights are more likelly) to find the index of the relevant item; return it...
    if (pos<=dm->weight_cum[0]) return 0;
    
    int low  = 0;
    int high = dm->exemplars-1;
    
    while (low+1<high)
    {
     // Select a split point...
      float wc_low = dm->weight_cum[low];
      float wc_high = dm->weight_cum[high];
      
      int split = (int)(low + (high - low)*((pos - wc_low) / (wc_high-wc_low)));
      
      if (split>=high) split = high-1;
      if (split<=low)  split = low+1;
      
     // Recurse to the relevant side...
      if (dm->weight_cum[split]>pos) high = split;
                                else low  = split;
    }
    
    return high;
  }
}



float * DataMatrix_to_int(DataMatrix * dm, float * external, float * internal)
{
 int i;
 
 // Switch on if conversion is required or not...
  if (dm->fv_conv==NULL)
  {
   internal = external;
  }
  else
  {
   // Loop and apply each conversion operation in turn...
    
    for (i=0; i<dm->ops_conv; i++)
    {
     dm->conv[i].conv->to_int(external + dm->conv[i].offset_external, internal + dm->conv[i].offset_internal);
    }
  }

 // Do the scale conversion...
  for (i=0; i<dm->feats_conv; i++) internal[i] *= dm->mult[i]; 
 
 // Return...
  return internal;
}


float * DataMatrix_to_ext(DataMatrix * dm, float * internal, float * external)
{
 int i;
 
 // Apply the inverse scale... 
  for (i=0; i<dm->feats_conv; i++) internal[i] /= dm->mult[i];
  
 // Escape early if no conversion required...
  if (dm->fv_conv==NULL) return internal;
  
 // Loop and apply each conversion operation in turn...
  for (i=0; i<dm->ops_conv; i++)
  {
   dm->conv[i].conv->to_ext(internal + dm->conv[i].offset_internal, external + dm->conv[i].offset_external);
  }
 
 // Return the converted array...
  return external;
}


size_t DataMatrix_byte_size(DataMatrix * dm)
{
 size_t mem = sizeof(DataMatrix);
 
 if (dm->dt!=NULL) mem += PyArray_NDIM(dm->array) * sizeof(DimType);
 if (dm->weight_cum!=NULL) mem += dm->exemplars * sizeof(float);
 if (dm->mult!=NULL) mem += dm->feats_conv * sizeof(float);
 if (dm->fv!=NULL) mem += dm->feats * sizeof(float);
 if (dm->feat_indices!=NULL) mem += dm->feat_dims * sizeof(int);
 if (dm->fv_conv!=NULL) mem += dm->feats_conv * sizeof(float);
 if (dm->conv!=NULL) mem += dm->ops_conv * sizeof(ConvertOp);

 return mem;  
}
