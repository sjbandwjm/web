let i = 0;
// while (true) {
    i++;
    let myHeading = document.querySelector("h1");
    myHeading.textContent = "Hello world!" + String(i);
    // alert("helo");
// }


// document.querySelector("html").addEventListener("click", function () {
//     alert("别戳我，我怕疼。");
// });


let myImage = document.querySelector("img");

myImage.onclick = function () {
  let mySrc = myImage.getAttribute("src");
  if (mySrc === "images/test_img1.jpg") {
    myImage.setAttribute("src", "images/test_img.jpg");
  } else {
    myImage.setAttribute("src", "images/test_img1.jpg");
  }
};


function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}