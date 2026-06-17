import os
import random
import string
import press_pb2 as press


def create_value_map(data_dict):
    value_map = press.ValueMap()
    for key, value in data_dict.items():
        if isinstance(value, str):
            value_map.value_map[key].s_value = value
        elif isinstance(value, int):
            value_map.value_map[key].i_value = value
        elif isinstance(value, float):
            value_map.value_map[key].d_value = value
        elif isinstance(value, bytes):
            value_map.value_map[key].b_value = value
        elif isinstance(value, list):
            if not value:
                pass
            else:
                first_element = value[0]
                if isinstance(first_element, str):
                    value_map.value_map[key].s_list.value.extend(value)
                elif isinstance(first_element, int):
                    value_map.value_map[key].i_list.value.extend(value)
                elif isinstance(first_element, float):
                    value_map.value_map[key].d_list.value.extend(value)
                elif isinstance(first_element, bytes):
                    value_map.value_map[key].b_list.value.extend(value)
    return value_map


def create_request(common_data, ad_data_list, label_data_list):
    request = press.Request()
    request.common_feat.CopyFrom(create_value_map(common_data))

    for ad_data in ad_data_list:
        ad_map = request.ad_feat.add()
        ad_map.CopyFrom(create_value_map(ad_data))

    for label_data in label_data_list:
        label_map = request.labels.add()
        label_map.CopyFrom(create_value_map(label_data))

    return request


def write_requests_to_file(requests, filename):
    collection = press.Requests()
    collection.requests.extend(requests)
    try:
        with open(filename, "wb") as f:
            f.write(collection.SerializeToString())
        print(f"多个 Request 已成功写入 {filename}")
    except Exception as e:
        print(f"写入文件时出错: {e}")


class RequestGenerator:
    MapSize = 10
    StringMin = 8
    StringMax = 16
    ListMin = 8
    ListMax = 16

    def __init__(self):
        self.counter = 0
        self.fixed_keys = [f"feature_key_{i:03d}" for i in range(self.MapSize)]

    def gen_random_request(self):
        self.counter += 1
        if self.counter % 1000 == 0:
            print("generate {}".format(self.counter))
        common = self.gen_fixed_value_map()
        ads = [self.gen_fixed_value_map() for _ in range(self.MapSize)]
        labels = [self.gen_fixed_value_map() for _ in range(self.MapSize)]
        return create_request(common, ads, labels)

    def gen_fixed_value_map(self):
        ret = {}
        for key in self.fixed_keys:
            ret[key] = self.gen_random_value()
        return ret

    def gen_random_value(self):
        choice = random.random()
        if choice < 0.5:
            return self.gen_random_uint_list()
        else:
            sub_choice = random.randint(1, 7)
            if sub_choice == 1:
                return self.gen_random_string()
            if sub_choice == 2:
                return self.gen_random_uint()
            if sub_choice == 3:
                return self.gen_random_float()
            if sub_choice == 4:
                return self.gen_random_bytes()
            if sub_choice == 5:
                return self.gen_random_string_list()
            if sub_choice == 6:
                return self.gen_random_float_list()
            if sub_choice == 7:
                return self.gen_random_bytes_list()

    def gen_random_string(self):
        characters = string.ascii_letters + string.digits
        length = random.randint(self.StringMin, self.StringMax)
        return ''.join(random.choice(characters) for _ in range(length))

    def gen_random_uint(self):
        selected_bytes = random.randint(1, 10)
        min_val = 0 if selected_bytes == 1 else (1 << ((selected_bytes - 1) * 7))
        if selected_bytes >= 10:
            max_val = (1 << 64) - 1
        else:
            max_val = (1 << (selected_bytes * 7)) - 1
        return random.randint(min_val, max_val)

    def gen_random_float(self):
        return random.uniform(-65536, 65536)

    def gen_random_bytes(self):
        return random.randbytes(random.randint(self.StringMin, self.StringMax))

    def gen_random_string_list(self):
        length = random.randint(self.ListMin, self.ListMax)
        return [self.gen_random_string() for _ in range(length)]

    def gen_random_uint_list(self):
        length = random.randint(self.ListMin, self.ListMax)
        return [self.gen_random_uint() for _ in range(length)]

    def gen_random_float_list(self):
        length = random.randint(self.ListMin, self.ListMax)
        return [self.gen_random_float() for _ in range(length)]

    def gen_random_bytes_list(self):
        length = random.randint(self.ListMin, self.ListMax)
        return [self.gen_random_bytes() for _ in range(length)]


def main():
    generator = RequestGenerator()
    requests = [generator.gen_random_request() for _ in range(10000)]
    pb_fp = "./request_data.pb"
    if os.path.exists(pb_fp):
        os.remove(pb_fp)
    write_requests_to_file(requests, pb_fp)


if __name__ == "__main__":
    main()