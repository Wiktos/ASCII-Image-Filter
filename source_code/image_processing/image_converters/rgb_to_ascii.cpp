#include "rgb_to_ascii.h"

[[nodiscard]] std::unique_ptr<AsciiImage> RGBToAscii::convert(const RGBImage& rgb_img) {
	std::lock_guard<std::mutex> lock(m_mutex);

	std::unique_ptr<GrayscaleImage> gray_img(RGBToGrayscale::get_instance().convert(rgb_img));
	ascii_img = std::make_unique<AsciiImage>(new AsciiImage(&rgb_img));

	ppaint_image(rgb_img, *gray_img.get());

	return std::move(ascii_img);
}

void RGBToAscii::ppaint_image(const RGBImage& rgb_img, const GrayscaleImage& gray_img) {
	static constexpr int GRAIN = 500;

	std::vector<std::future<void>> pool;

	for (int i = 0; i < gray_img.rows(); i += GRAIN) {
		if (i + GRAIN < gray_img.rows()) {
			pool.push_back(std::async(std::launch::async, &RGBToAscii::paint_fragment, this, std::ref(rgb_img), std::ref(gray_img), i, i + GRAIN));
		}
		else {
			pool.push_back(std::async(std::launch::async, &RGBToAscii::paint_fragment, this, std::ref(rgb_img), std::ref(gray_img), i, gray_img.rows()));
		}
	}

	//wait untill all threads finish
	for (uint i = 0; i < pool.size(); i++) {
		pool[i].get();
	}
}

void RGBToAscii::paint_fragment(const RGBImage& rgb_img, const GrayscaleImage& gray_img, int low_row, int high_row) {
	for (int i = 0; i < gray_img.cols(); i += SUBSPACE_LENGTH) {
		for (int j = low_row; j < high_row; j += SUBSPACE_LENGTH) {

			if (i + SUBSPACE_LENGTH < gray_img.cols() && j + SUBSPACE_LENGTH < gray_img.rows()) {
				print_ascii({ j, i }, { j + SUBSPACE_LENGTH, i + SUBSPACE_LENGTH }, rgb_img, gray_img);
			}

		}
	}
}

void RGBToAscii::print_ascii(cv::Point upper_left, cv::Point lower_right, const RGBImage& rgb_img, const GrayscaleImage& gray_img) {

	int subspace_width = lower_right.x - upper_left.x;
	int subspace_height = lower_right.y - upper_left.y;

	auto[mean_ascii, color] = mean_values(upper_left, lower_right, rgb_img, gray_img);
	std::string ascii = as_string(mean_ascii);
	double scale = compute_scale(ascii, subspace_width, subspace_height);

	ascii_img->put_text(ascii, cv::Point(lower_right.y, upper_left.x), cv::FONT_HERSHEY_SIMPLEX, scale, color);
}

std::pair<char, cv::Vec3b> RGBToAscii::mean_values(cv::Point upper_left, cv::Point lower_right, const RGBImage& rgb_img, const GrayscaleImage& gray_img) {
	int mean_red = 0;
	int mean_blue = 0;
	int mean_green = 0;
	int mean_val = 0;

	//compute mean values
	for (int i = upper_left.y; i < lower_right.y; i++) {
		for (int j = upper_left.x; j < lower_right.x; j++) {
			//col
			auto curr_col = rgb_img.pixel_at(j, i);
			mean_blue += curr_col[0];
			mean_green += curr_col[1];
			mean_red += curr_col[2];

			//ascii val
			auto curr_val = gray_img.pixel_at(j, i);
			mean_val += curr_val;
		}
	}

	int x_cnt = lower_right.x - upper_left.x;
	int y_cnt = lower_right.y - upper_left.y;
	mean_blue /= (x_cnt * y_cnt);
	mean_green /= (x_cnt * y_cnt);
	mean_red /= (x_cnt * y_cnt);
	mean_val /= (x_cnt * y_cnt);

	cv::Vec3b mean_col = { (uchar)mean_blue, (uchar)mean_green, (uchar)mean_red };
	char mean_ascii = mean_val % 127 + 33;

	return std::make_pair<>(mean_ascii, mean_col);
}

std::string RGBToAscii::as_string(char c) {
	std::stringstream ss;
	ss << c;
	std::string ascii = ss.str();
	return ascii;
}

double RGBToAscii::compute_scale(const std::string& ascii, int subspace_width, int subspace_height) {

	auto size = cv::getTextSize(ascii, cv::FONT_HERSHEY_SIMPLEX, 1, 1, 0);

	double retv = size.width > size.height ?
		static_cast<double>(subspace_width) / size.width : static_cast<double>(subspace_height) / size.height;

	return retv;
}