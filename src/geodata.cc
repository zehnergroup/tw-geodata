#include <node.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

// GEODATA FILE FORMAT:
//
// this is a custom file format specifically for low memory high performance hit tests on a set of polygons
//
// FILE LAYOUT
// unsigned char header[4] = "GEO!"
// unsigned int num_polygons = <number of polygons that follow>
// <POLYGON DATA>
//
// POLYGON LAYOUT
// unsigned int num_coordinates = <number of coordinates that follow>
// <COORDINATE DATA>
//
// COORDINATE LAYOUT
// double longitude
// double latitude
//

using namespace v8;

// C IMPL

typedef struct {
    double lng;
    double lat;
} geo_data_coordinate;
typedef struct {
    unsigned int num_polygons;
    uint8_t *polygons;
} geo_data;


int geo_data_hit_test(geo_data *data, double lng, double lat);
int geo_data_hit_test(geo_data *data, double lng, double lat) {
	if(!data) {
		return 0;
	}
    uint8_t *polygon_ptr = data->polygons;
    for(unsigned int n = 0; n < data->num_polygons; ++n) {
        unsigned int num_coordinates = *(unsigned int *)polygon_ptr; polygon_ptr += sizeof(unsigned int);
        geo_data_coordinate *coordinates = (geo_data_coordinate *)polygon_ptr;
        
        int c = 0;
        int i = -1;
        int l = num_coordinates;
        int j = l - 1;
        while(++i < l) {
            if(((coordinates[i].lng <= lng && lng < coordinates[j].lng) ||
                (coordinates[j].lng <= lng && lng < coordinates[i].lng)) &&
               (lat < (coordinates[j].lat - coordinates[i].lat) * (lng - coordinates[i].lng) / (coordinates[j].lng - coordinates[i].lng) + coordinates[i].lat)) {
                c = !c;
            }
            j = i;
        }
        
        if(c) {
            return 1;
        }
        
        polygon_ptr += num_coordinates * sizeof(geo_data_coordinate);
    }
    return 0;
};

void geo_data_destroy(geo_data *data);
void geo_data_destroy(geo_data *data) {
    if(data) {
        free(data->polygons);
        free(data);
    }
}

geo_data* geo_data_create(const char *filepath, int *status);
geo_data* geo_data_create(const char *filepath, int *status) {
    FILE *handle = fopen(filepath, "r");
    if(handle == NULL) {
        if(status) *status = -1000;
        ThrowException(Exception::TypeError(String::New(filepath)));
        return NULL;
    }
    
    if(fseek(handle, 0, SEEK_END)) {
        fclose(handle);
        if(status) *status = -1001;
        return NULL;
    }
    unsigned int len = (unsigned int)ftell(handle);
    if(fseek(handle, 0, SEEK_SET)) {
        fclose(handle);
        if(status) *status = -1002;
        return NULL;
    }
    
    if(len < sizeof(unsigned char) * 4 + sizeof(unsigned int)) {
        fclose(handle);
        if(status) *status = -1003;
        return NULL;
    }
    
    // verify header
    unsigned char header[4];
    if(!fread(header, 1, sizeof(unsigned char) * 4, handle)) {
        fclose(handle);
        if(status) *status = -1004;
        return NULL;
    }
    if(header[0] != 'G' || header[1] != 'E' || header[2] != 'O' || header[3] != '!') {
        fclose(handle);
        if(status) *status = -1005;
        return NULL;
    }
    
    // get num polygons
    unsigned int num_polygons = 0;
    if(!fread(&num_polygons, 1, sizeof(unsigned int), handle)) {
        fclose(handle);
        if(status) *status = -1006;
        return NULL;
    }
    
    // create geodata
    geo_data *data = (geo_data *)malloc(sizeof(geo_data));
    data->num_polygons = num_polygons;
    
    // no polygons
    if(data->num_polygons == 0) {
        fclose(handle);
        return data;
    }
    
    // allocate buffer
    unsigned int buffer_len = len - (sizeof(unsigned char) * 4 + sizeof(unsigned int));
    void *buffer = malloc(buffer_len);
    if(!buffer) {
        fclose(handle);
        if(status) *status = -1007;
        return NULL;
    }
    
    // assign buffer
    data->polygons = (uint8_t *)buffer;
    
    // read remaining data
    if(!fread(data->polygons, 1, buffer_len, handle)) {
        geo_data_destroy(data);
        fclose(handle);
        if(status) *status = -1008;
        return NULL;
    }
    
    // verify data
    unsigned int offset = 0;
    uint8_t *polygon_ptr = data->polygons;
    for(unsigned int i = 0; i < data->num_polygons; ++i) {
        unsigned int polygon_len = sizeof(unsigned int);
        
        // get num_coordinates
        if(offset + polygon_len > buffer_len) {
            geo_data_destroy(data);
            if(status) *status = -1009;
            return NULL;
        }
        unsigned int num_coordinates = *(unsigned int *)polygon_ptr;
        polygon_len += num_coordinates * sizeof(double) * 2;
        if(offset + polygon_len > buffer_len) {
            geo_data_destroy(data);
            if(status) *status = -1010;
            return NULL;
        }
        offset += polygon_len;
        polygon_ptr += polygon_len;
    }
    
    return data;
};

// C++ HORRIBLENESS

// HEADER

class GeoData : public node::ObjectWrap {
public:
	static void Init(Handle<Object> exports, Handle<Object> module);
private:
	explicit GeoData(const char *filepath);
	~GeoData();

	static Handle<Value> New(const Arguments& args);
	static Handle<Value> Contains(const Arguments& args);
	static Persistent<Function> constructor;
	geo_data *geo_data_;
};

// IMPL

Persistent<Function> GeoData::constructor;
GeoData::GeoData(const char *filepath) {
	if(filepath) {
        int status = 0;
		this->geo_data_ = geo_data_create(filepath, &status);
        
        if(status <= 0) {
            const char *msg = NULL;
            switch(status) {
                case -1000:
                    msg = strerror(errno);
                    break;
                case -1001:
                    msg = "-1001";
                    break;
                case -1002:
                    msg = "-1002";
                    break;
                case -1003:
                    msg = "-1003";
                    break;
                case -1004:
                    msg = "-1004";
                    break;
                case -1005:
                    msg = "-1005";
                    break;
                case -1006:
                    msg = "-1006";
                    break;
                case -1007:
                    msg = "-1007";
                    break;
                case -1008:
                    msg = "-1008";
                    break;
                case -1009:
                    msg = "-1009";
                    break;
                case -1010:
                    msg = "-1010";
                    break;
                default:
                    msg = "Unknown";
                    break;
            }
            ThrowException(Exception::TypeError(String::New(msg)));
        }
        
	} else {
		this->geo_data_ = NULL;
	}
}
GeoData::~GeoData() {
	if(this->geo_data_) {
		geo_data_destroy(this->geo_data_);
	}
}
Handle<Value> GeoData::New(const Arguments& args) {
	HandleScope scope;
	if(args.IsConstructCall()) {
		const char *filepath = (args[0]->IsUndefined()) ? NULL : *String::Utf8Value(args[0]->ToString());
		GeoData *obj = new GeoData(filepath);
		obj->Wrap(args.This());
		return args.This();
	} else {
		const int argc = 1;
		Local<Value> argv[argc] = {args[0]};
		return scope.Close(constructor->NewInstance(argc, argv));
	}
}
Handle<Value> GeoData::Contains(const Arguments& args) {
	HandleScope scope;
	
	double lng = args[0]->IsUndefined() ? -320.0 : args[0]->NumberValue();
	double lat = args[1]->IsUndefined() ? -320.0 : args[1]->NumberValue();

	if(lng < -180.0 || lng > 180.0 || lat < -180.0 || lat > 180.0) {
		return scope.Close(Boolean::New(false));
	}

	GeoData *obj = node::ObjectWrap::Unwrap<GeoData>(args.This());

	if(geo_data_hit_test(obj->geo_data_, lng, lat)) {
		return scope.Close(Boolean::New(true));
	} else {
		return scope.Close(Boolean::New(false));
	}
}

void GeoData::Init(Handle<Object> exports, Handle<Object> module) {
	// template
	Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
	tpl->SetClassName(String::NewSymbol("GeoData"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	// prototype
	tpl->PrototypeTemplate()->Set(String::NewSymbol("contains"),
		FunctionTemplate::New(Contains)->GetFunction());
	constructor = Persistent<Function>::New(tpl->GetFunction());

	// module
	module->Set(String::NewSymbol("exports"), constructor);
}

void Init(Handle<Object> exports, Handle<Object> module) {
	GeoData::Init(exports, module);
}
NODE_MODULE(geodata, Init);