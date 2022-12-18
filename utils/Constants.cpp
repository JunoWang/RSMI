#include "Constants.h"

#ifdef __APPLE__
// const string Constants::RECORDS = "/Users/guanli/Dropbox/records/VLDB20/";
const string Constants::QUERYPROFILES = "/Users/guanli/Documents/datasets/RLRtree/queryprofile/";
// const string Constants::DATASETS = "/Users/guanli/Documents/datasets/RLRtree/raw/";
const string Constants::RECORDS = "./files/records/";
// const string Constants::QUERYPROFILES = "./files/queryprofile/";
const string Constants::DATASETS = "./datasets/";
#else
// const string Constants::RECORDS = "/home/liuguanli/Dropbox/records/VLDB20/";
// const string Constants::QUERYPROFILES = "./files/queryprofile/";
// const string Constants::DATASETS = "/home/liuguanli/Documents/datasets/RLRtree/raw/";
const string Constants::RECORDS = "/home/cwang39/RSMI/files/records/";
const string Constants::QUERYPROFILES = "/home/cwang39/RSMI/files/queryprofile/";
const string Constants::DATASETS = "/home/cwang39/RSMI/datasets/";
#endif
const string Constants::DEFAULT_DISTRIBUTION = "skewed";
const string Constants::BUILD = "build/";
const string Constants::UPDATE = "update/";
const string Constants::POINT = "point/";
const string Constants::WINDOW = "window/";
const string Constants::ACCWINDOW = "accwindow/";
const string Constants::KNN = "knn/";
const string Constants::ACCKNN = "accknn/";
const string Constants::INSERT = "insert/";
const string Constants::DELETE = "delete/";
const string Constants::INSERTPOINT = "insertPoint/";
const string Constants::INSERTWINDOW = "insertWindow/";
const string Constants::INSERTACCWINDOW = "insertAccWindow/";
const string Constants::INSERTKNN = "insertKnn/";
const string Constants::INSERTACCKNN= "insertAccKnn/";
const string Constants::DELETEPOINT= "delete_point/";
const string Constants::DELETEWINDOW= "deleteWindow/";
const string Constants::DELETEACCWINDOW= "deleteAccWindow/";
const string Constants::DELETEKNN= "deleteKnn/";
const string Constants::DELETEACCKNN= "deleteAccKnn/";
const string Constants::LEARNED_CDF= "learned_cdf/";


const string Constants::TORCH_MODELS = "/home/cwang39/RSMI/torch_models/";
const string Constants::TORCH_MODELS_ZM = "/home/cwang39/RSMI/torch_models_zm/";

const string Constants::PRE_TRAIN_DATA = "/home/cwang39/RSMI/pre_train/2D_data/";
const string Constants::PRE_TRAIN_1D_DATA = "/home/cwang39/RSMI/pre_train/1D_data/";

const string Constants::SYNTHETIC_SFC_Z = "/home/cwang39/RSMI/pre_train/sfc_z/";
const string Constants::SFC_Z_WEIGHT = "/home/cwang39/RSMI/pre_train/sfc_z_weight/";
const string Constants::SFC_Z_COUNT = "/home/cwang39/RSMI/pre_train/sfc_z_count/";

const string Constants::FEATURES_PATH_ZM = "/home/cwang39/RSMI/pre_train/features_zm/";
const string Constants::PRE_TRAIN_MODEL_PATH_ZM = "/home/cwang39/RSMI/pre_train/models_zm/";

const string Constants::FEATURES_PATH_RSMI = "/home/cwang39/RSMI/pre_train/features_rsmi/";
const string Constants::PRE_TRAIN_MODEL_PATH_RSMI = "/home/cwang39/RSMI/pre_train/models_rsmi/";

const double Constants::LEARNING_RATE = 0.05;

// const double Constants::MODEL_REUSE_THRESHOLD = 0.1;
// const double Constants::SAMPLING_RATE = 0.01;

Constants::Constants()
{
}